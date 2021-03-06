#ifndef DISABLE_VISP
//! needed to avoid a weird bug in ViSP
#define PNG_SKIP_SETJMP_CHECK
#endif
#ifdef _WIN32
#define hypot _hypot
#endif
#include "mtf/mtf.h"
#include "mtf/Config/parameters.h"
#include "mtf/Config/datasets.h"
#include "mtf/Utilities/miscUtils.h"
//! tools for reading in images from various sources like image sequences, 
//! videos and cameras as well as for pre processing them
#include "mtf/Tools/pipeline.h"

#include <time.h>
#include <string.h>
#include <vector>
#include <memory>

#include "opencv2/core/core.hpp"
#include "opencv2/highgui/highgui.hpp"

#include <Python.h>
#include <numpy/arrayobject.h>

using namespace std;
using namespace mtf::params;

static PyObject* initInput(PyObject* self, PyObject* args);
static PyObject* initialize(PyObject* self, PyObject* args);
static PyObject* update(PyObject* self, PyObject* args);
static PyObject* setRegion(PyObject* self, PyObject* args);
static PyObject* getRegion(PyObject* self, PyObject* args);

static PyArrayObject *img_py;
static PyArrayObject *init_corners_py;
static PyArrayObject *out_corners_py;
static double* out_corners_data;

typedef unique_ptr<mtf::TrackerBase> Tracker_;
static vector<Tracker_> trackers;
static vector<PreProc_> pre_proc;
static Input_ input;

static double min_x, min_y, max_x, max_y;
static double size_x, size_y;
static cv::Mat init_img_cv, init_corners_cv, curr_img_cv;

static int img_height, img_width;
static vector<cv::Scalar> obj_cols;

static cv::Point fps_origin(10, 20);
static double fps_font_size = 0.50;
static cv::Scalar fps_color(0, 255, 0);
static int frame_id;
static char* config_root_dir = "C++/MTF/Config";
bool using_input_pipeline = false;

static PyMethodDef pyMTFMethods[] = {
	{ "initInput", initInput, METH_VARARGS },
	{ "initialize", initialize, METH_VARARGS },
	{ "update", update, METH_VARARGS },
	{ "setRegion", setRegion, METH_VARARGS },
	{ "getRegion", getRegion, METH_VARARGS },
	{ NULL, NULL }     /* Sentinel - marks the end of this structure */
};

/* ==== Initialize the C_test functions ====================== */
// Module name must be pyMTF in compile and linked
PyMODINIT_FUNC initpyMTF()  {
	(void)Py_InitModule("pyMTF", pyMTFMethods);
	import_array();  // Must be present for NumPy.  Called first after above line.
}
/* ==== initialize input pipeline ==== */
static PyObject* initInput(PyObject* self, PyObject* args) {
#ifdef USE_TBB
	Eigen::initParallel();
#endif
	if(!readParams(0, nullptr)){
		return Py_BuildValue("i", 0);
	}
	input.reset(getInput(pipeline));
	if(!input->initialize()){
		printf("Pipeline could not be initialized successfully. Exiting...\n");
		return Py_BuildValue("i", 0);
	}
	CVUtils cv_utils(img_resize_factor);
	if(!getObjectsToTrack(cv_utils, input.get())){
		printf("Object(s) to be tracked could not be read\n");
		return Py_BuildValue("i", 0);
	}
	size_x = cv_utils.getObj(0).size_x;
	size_y = cv_utils.getObj(0).size_y;
	max_x = cv_utils.getObj(0).max_point.x;
	max_y = cv_utils.getObj(0).max_point.y;
	min_x = cv_utils.getObj(0).min_point.x;
	min_y = cv_utils.getObj(0).min_point.y;

	init_corners_cv = cv_utils.getObj(0).corners.clone();
	init_img_cv = input->getFrame();		
	img_height = input->getHeight();
	img_width = input->getWidth();

	using_input_pipeline = true;
	return Py_BuildValue("i", 1);
}

/* ==== initialize tracker ==== */
static PyObject* initialize(PyObject* self, PyObject* args) {
	if(!using_input_pipeline){
		/*parse first input array*/
		if(!PyArg_ParseTuple(args, "O!O!z", &PyArray_Type, &img_py, &PyArray_Type,
			&init_corners_py, &config_root_dir)) {
			printf("\n----pyMTF::initialize: Input arguments could not be parsed----\n\n");
			return Py_BuildValue("i", 0);
		}

		if(img_py == NULL) {
			printf("\n----pyMTF::initialize::init_img is NULL----\n\n");
			return Py_BuildValue("i", 0);
		}
		if(init_corners_py == NULL) {
			printf("\n----pyMTF::initialize::init_corners is NULL----\n\n");
			return Py_BuildValue("i", 0);
		}

		if(init_corners_py->dimensions[0] != 2 || init_corners_py->dimensions[1] != 4){
			printf("pyMTF::Initial corners matrix has incorrect dimensions: %ld, %ld\n",
				init_corners_py->dimensions[0], init_corners_py->dimensions[1]);
			return Py_BuildValue("i", 0);
		}
#ifdef USE_TBB
		Eigen::initParallel();
#endif
		if(!config_root_dir){
			config_root_dir = "C++/MTF/Config";
			printf("Using default configuration folder: %s\n", config_root_dir);
		} else{
			printf("Reading MTF configuration files from: %s\n", config_root_dir);
		}
		config_dir = std::string(config_root_dir);

		img_height = img_py->dimensions[0];
		img_width = img_py->dimensions[1];

		printf("img_height: %d\n", img_height);
		printf("img_width: %d\n", img_width);

		init_img_cv=cv::Mat(img_height, img_width, CV_8UC3, img_py->data);

		cv::Mat temp(2, 4, CV_64FC1, init_corners_py->data);
		init_corners_cv.create(2, 4, CV_64FC1);

		//double *in_corners_data = (double*)init_corners_py->data;
		//for(int i = 0; i < 4; i++) {
		//	init_corners_cv.at<double>(0, i) = in_corners_data[i];
		//	init_corners_cv.at<double>(1, i) = in_corners_data[i + init_corners_py->strides[1]];
		//}
		init_corners_cv.at<double>(0, 0) = temp.at<double>(0, 0);
		init_corners_cv.at<double>(1, 0) = temp.at<double>(0, 1);
		init_corners_cv.at<double>(0, 1) = temp.at<double>(0, 2);
		init_corners_cv.at<double>(1, 1) = temp.at<double>(0, 3);
		init_corners_cv.at<double>(0, 2) = temp.at<double>(1, 0);
		init_corners_cv.at<double>(1, 2) = temp.at<double>(1, 1);
		init_corners_cv.at<double>(0, 3) = temp.at<double>(1, 2);
		init_corners_cv.at<double>(1, 3) = temp.at<double>(1, 3);
		
		printf("init_corners_cv:\n");
		for(unsigned int corner_id = 0; corner_id < 4; ++corner_id) {
			printf("%d: (%f, %f)\n", corner_id, init_corners_cv.at<double>(0, corner_id), init_corners_cv.at<double>(1, corner_id));
		}

		min_x = init_corners_cv.at<double>(0, 0);
		min_y = init_corners_cv.at<double>(1, 0);
		max_x = init_corners_cv.at<double>(0, 2);
		max_y = init_corners_cv.at<double>(1, 2);
		size_x = max_x - min_x;
		size_y = max_y - min_y;

		if(!readParams(0, nullptr)){ return Py_BuildValue("i", 0); }

		printf("*******************************\n");
		printf("Using parameters:\n");
		printf("n_trackers: %d\n", n_trackers);
		printf("actor_id: %d\n", actor_id);
		printf("source_id: %d\n", seq_id);
		printf("source_name: %s\n", seq_name.c_str());
		printf("actor: %s\n", actor.c_str());
		printf("pipeline: %c\n", pipeline);
		printf("img_source: %c\n", img_source);
		printf("show_cv_window: %d\n", show_cv_window);
		printf("read_obj_from_gt: %d\n", read_obj_from_gt);
		printf("write_tracking_data: %d\n", write_tracking_data);
		printf("mtf_sm: %s\n", mtf_sm);
		printf("mtf_am: %s\n", mtf_am);
		printf("mtf_ssm: %s\n", mtf_ssm);
		printf("*******************************\n");
	}


	/*********************************** initialize trackers ***********************************/
	if(n_trackers > 1){
		printf("Multi tracker setup enabled\n");
		write_tracking_data = 0;
	}
	if(res_from_size){
		printf("Getting sampling resolution from object size...\n");
	}
	FILE *multi_fid = NULL;
	trackers.resize(n_trackers);
	pre_proc.resize(n_trackers);
	for(unsigned int tracker_id = 0; tracker_id < n_trackers; ++tracker_id) {
		try{
			if(n_trackers > 1){
				multi_fid = readTrackerParams(multi_fid);
			}
			if(res_from_size){
				resx = static_cast<unsigned int>(size_x / res_from_size);
				resy = static_cast<unsigned int>(size_y / res_from_size);
			}
			trackers[tracker_id].reset(mtf::getTracker(mtf_sm, mtf_am, mtf_ssm, mtf_ilm));
			if(!trackers[tracker_id]){
				printf("Tracker could not be created successfully\n");
				return Py_BuildValue("i", 0);
			}
		} catch(const mtf::utils::Exception &err){
			printf("Exception of type %s encountered while creating the tracker: %s\n", 
				err.type(), err.what());
			return Py_BuildValue("i", 0);
		}
		try{
			pre_proc[tracker_id] = getPreProc(trackers[tracker_id]->inputType(), pre_proc_type);
			pre_proc[tracker_id]->initialize(init_img_cv);
		} catch(const mtf::utils::Exception &err){
			printf("Exception of type %s encountered while initializing the pre processor: %s\n",
				err.type(), err.what());
			return Py_BuildValue("i", 0);
		}
		try{
			for(PreProc_ curr_obj = pre_proc[tracker_id]; curr_obj; curr_obj = curr_obj->next){
				trackers[tracker_id]->setImage(curr_obj->getFrame());
			}
			printf("Initializing tracker %d with object of size %f x %f\n", tracker_id,
				size_x, size_y);
			trackers[tracker_id]->initialize(init_corners_cv);
		} catch(const mtf::utils::Exception &err){
			printf("Exception of type %s encountered while initializing the tracker: %s\n", 
				err.type(), err.what());
			return Py_BuildValue("i", 0);
		}		
	}
	if(show_cv_window) {
		cv::namedWindow("PyMTF", cv::WINDOW_AUTOSIZE);
		obj_cols.push_back(cv::Scalar(0, 0, 255));
		obj_cols.push_back(cv::Scalar(0, 255, 0));
		obj_cols.push_back(cv::Scalar(255, 0, 0));
		obj_cols.push_back(cv::Scalar(255, 255, 0));
		obj_cols.push_back(cv::Scalar(255, 0, 255));
		obj_cols.push_back(cv::Scalar(0, 255, 255));
		obj_cols.push_back(cv::Scalar(255, 255, 255));
		obj_cols.push_back(cv::Scalar(0, 0, 0));
	}
	int dims[] = { 2, 4 };
	out_corners_py = (PyArrayObject *)PyArray_FromDims(2, dims, NPY_DOUBLE);
	out_corners_data = (double*)out_corners_py->data;

	frame_id = 0;

	return Py_BuildValue("i", 1);
}

static PyObject* update(PyObject* self, PyObject* args) {
	if(!using_input_pipeline){
		/*parse first input array*/
		if(!PyArg_ParseTuple(args, "O!", &PyArray_Type, &img_py)) {
			printf("\n----pyMTF::update: Input arguments could not be parsed----\n\n");
			return Py_BuildValue("i", 0);
		}
		if(img_py == NULL) {
			printf("\n----pyMTF::img_py is NULL----\n\n");
			return Py_BuildValue("i", 0);
		}
		curr_img_cv = cv::Mat(img_height, img_width, CV_8UC3, img_py->data);
	} else{
		if(!input->update()){
			printf("Frame %d could not be read from the input pipeline\n", input->getFrameID() + 1);
			return Py_BuildValue("i", 0);
		}
		curr_img_cv = input->getFrame();
	}
	double fps=0, fps_win=0;
	double tracking_time, tracking_time_with_input;
	++frame_id;
	mtf_clock_get(start_time_with_input);
	//update trackers
	for(unsigned int tracker_id = 0; tracker_id < n_trackers; ++tracker_id) {
		try{
			//! update pre processor
			pre_proc[tracker_id]->update(curr_img_cv);
			mtf_clock_get(start_time);
			//! update tracker
			trackers[tracker_id]->update();
			mtf_clock_get(end_time);
			mtf_clock_measure(start_time, end_time, tracking_time);
			mtf_clock_measure(start_time_with_input, end_time, tracking_time_with_input);
			fps = 1.0 / tracking_time;
			fps_win = 1.0 / tracking_time_with_input;
			if(reset_template){
				trackers[tracker_id]->initialize(trackers[tracker_id]->getRegion());
			}
		} catch(const mtf::utils::Exception &err){
			printf("Exception of type %s encountered while updating the tracker: %s\n",
				err.type(), err.what());
			return Py_BuildValue("i", 0);
		}
	}	
	if(show_cv_window) {
		/* draw tracker positions to OpenCV window */
		for(unsigned int tracker_id = 0; tracker_id < n_trackers; ++tracker_id) {
			int col_id = tracker_id % obj_cols.size();
			cv::Point2d corners[4];
			mtf::utils::Corners(trackers[tracker_id]->getRegion()).points(corners);
			line(curr_img_cv, corners[0], corners[1], obj_cols[col_id], line_thickness);
			line(curr_img_cv, corners[1], corners[2], obj_cols[col_id], line_thickness);
			line(curr_img_cv, corners[2], corners[3], obj_cols[col_id], line_thickness);
			line(curr_img_cv, corners[3], corners[0], obj_cols[col_id], line_thickness);
			putText(curr_img_cv, trackers[tracker_id]->name, corners[0],
				cv::FONT_HERSHEY_SIMPLEX, fps_font_size, obj_cols[col_id]);
		}
		std::string fps_text=cv::format("frame: %d c: %12.6f cw: %12.6f", frame_id, fps, fps_win);
		putText(curr_img_cv, fps_text, fps_origin, cv::FONT_HERSHEY_SIMPLEX, fps_font_size, fps_color);
		imshow("PyMTF", curr_img_cv);
		cv::waitKey(1);
	}
	cv::Mat out_corners = trackers[0]->getRegion();
	for(unsigned int corner_id = 0; corner_id < 4; corner_id++) {
		out_corners_data[corner_id] = out_corners.at<double>(0, corner_id);
		out_corners_data[corner_id + 4] = out_corners.at<double>(1, corner_id);
	}
	return Py_BuildValue("O", out_corners_py);
}
static PyObject* setRegion(PyObject* self, PyObject* args) {
	/*parse first input array*/
	if(!PyArg_ParseTuple(args, "O!", &PyArray_Type, &init_corners_py)) {
		printf("\n----pyMTF::initialize: Input arguments could not be parsed----\n\n");
		return Py_BuildValue("i", 0);
	}

	if(init_corners_py == NULL) {
		printf("\n----pyMTF::initialize::init_corners is NULL----\n\n");
		return Py_BuildValue("i", 0);
	}

	if(init_corners_py->dimensions[0] != 2 || init_corners_py->dimensions[1] != 4){
		printf("pyMTF::Initial corners matrix has incorrect dimensions: %ld, %ld\n",
			init_corners_py->dimensions[0], init_corners_py->dimensions[1]);
		return Py_BuildValue("i", 0);
	}
	cv::Mat corners(2, 4, CV_64FC1, init_corners_py->data);
	trackers[0]->setRegion(corners);
	return Py_BuildValue("i", 1);

}
static PyObject* getRegion(PyObject* self, PyObject* args){
	return Py_BuildValue("O", out_corners_py);
}
