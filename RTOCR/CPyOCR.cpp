/*************************************************************

    程序名称:基于Python3原生C接口的OCR C++类(阻塞)
    程序版本:REV 0.1
    创建日期:20231224
    设计编写:rainhenry
    作者邮箱:rainhenry@savelife-tech.com
    版    权:SAVELIFE Technology of Yunnan Co., LTD
    开源协议:GPL

    版本修订
        REV 0.1   20231224      rainhenry    创建文档

*************************************************************/

//  包含头文件
#include <stdio.h>
#include <stdlib.h>

#include "CPyOCR.h"

#include <Python.h>
#include <numpy/arrayobject.h>
#include <numpy/ndarrayobject.h>
#include <numpy/npy_3kcompat.h>

//  初始化全局变量
bool CPyOCR::Py_Initialize_flag = false;

//  构造函数
CPyOCR::CPyOCR()
{
    //  当没有初始化过
    if(!Py_Initialize_flag)
    {
        if(!Py_IsInitialized())
        {
            Py_Initialize();
            import_array_init();
        }
        Py_Initialize_flag = true;   //  标记已经初始化
    }

    //  初始化私有数据
    py_module                 = nullptr;
#if USE_OPENVINO
    py_get_compile_model      = nullptr;
    py_get_model_output_layer = nullptr;
    py_det_model_handle       = nullptr;
    py_rec_model_handle       = nullptr;
    py_det_out_layer_handle   = nullptr;
    py_rec_out_layer_handle   = nullptr;
    py_dict_handle            = nullptr;
#else
    py_ocr_init_fast          = nullptr;
    py_ocr_init_precise       = nullptr;
    py_ocr_fast_handle        = nullptr;
    py_ocr_precise_handle     = nullptr;
#endif
    py_ocr_ex                 = nullptr;
}

//  析构函数
CPyOCR::~CPyOCR()
{
    //  此处不可以调用Release，因为Python环境实际运行所在线程
    //  不一定和构造该类对象是同一个线程
}

//  释放资源
//  注意！该释放必须和执行本体在同一线程中！
void CPyOCR::Release(void)
{
    if(py_ocr_ex != nullptr)
    {
        Py_DecRef(static_cast<PyObject*>(py_ocr_ex));
    }

#if USE_OPENVINO
    if(py_dict_handle != nullptr)
    {
        Py_DecRef(static_cast<PyObject*>(py_dict_handle));
    }
    if(py_rec_out_layer_handle != nullptr)
    {
        Py_DecRef(static_cast<PyObject*>(py_rec_out_layer_handle));
    }
    if(py_det_out_layer_handle != nullptr)
    {
        Py_DecRef(static_cast<PyObject*>(py_det_out_layer_handle));
    }
    if(py_rec_model_handle != nullptr)
    {
        Py_DecRef(static_cast<PyObject*>(py_rec_model_handle));
    }
    if(py_det_model_handle != nullptr)
    {
        Py_DecRef(static_cast<PyObject*>(py_det_model_handle));
    }
    if(py_get_model_output_layer != nullptr)
    {
        Py_DecRef(static_cast<PyObject*>(py_get_model_output_layer));
    }
    if(py_get_compile_model != nullptr)
    {
        Py_DecRef(static_cast<PyObject*>(py_get_compile_model));
    }
#else
    if(py_ocr_init_precise != nullptr)
    {
        Py_DecRef(static_cast<PyObject*>(py_ocr_init_precise));
    }
    if(py_ocr_init_fast != nullptr)
    {
        Py_DecRef(static_cast<PyObject*>(py_ocr_init_fast));
    }
    if(py_ocr_precise_handle != nullptr)
    {
        Py_DecRef(static_cast<PyObject*>(py_ocr_precise_handle));
    }
#endif

    if(py_module != nullptr)
    {
        Py_DecRef(static_cast<PyObject*>(py_module));
    }

    if(Py_Initialize_flag)
    {
        Py_Finalize();
        Py_Initialize_flag = false;   //  标记未初始化
    }
}

//  为了兼容Python C的原生API，独立封装numpy的C初始化接口
int CPyOCR::import_array_init(void)
{
    import_array()
    return 0;
}

//  初始化
//  注意！该初始化必须和执行本体在同一线程中！
void CPyOCR::Init(void)
{
    //  开启Python线程支持
    PyEval_InitThreads();
    PyEval_SaveThread();

    //  检测当前线程是否拥有GIL
    int ck = PyGILState_Check() ;
    if (!ck)
    {
        PyGILState_Ensure(); //  如果没有GIL，则申请获取GIL
    }

    //  构造基本Python环境
    PyRun_SimpleString("import sys");
    PyRun_SimpleString("sys.path.append('../RTOCR')");

    //  载入rtocr.py文件
#if USE_OPENVINO
    py_module = static_cast<void*>(PyImport_ImportModule("openvino_ocr"));
#else
    py_module = static_cast<void*>(PyImport_ImportModule("rtocr"));
#endif

    //  检查是否成功
    if(py_module == nullptr)
    {
        printf("[Error] py_module == null!!");
        return;
    }

#if USE_OPENVINO
    //  获取编译后的检测模型
    py_get_compile_model = static_cast<void*>(PyObject_GetAttrString(static_cast<PyObject*>(py_module), "get_compile_model"));
    PyObject* py_args = PyTuple_New(1);
    PyObject* py_value = Py_BuildValue("s", "../ch_PP-OCRv3_det_infer/inference.pdmodel");
    PyTuple_SetItem(py_args, 0, py_value);
    py_det_model_handle = static_cast<void*>(PyObject_CallObject(static_cast<PyObject*>(py_get_compile_model), py_args));
    if(py_det_model_handle == nullptr)
    {
        printf("py_det_model_handle == nullptr\n");
        printf("Not Found OpenVINO!!\n");
        return;
    }

    //  获取检测输出层
    py_get_model_output_layer = static_cast<void*>(PyObject_GetAttrString(static_cast<PyObject*>(py_module), "get_model_output_layer"));
    py_args = PyTuple_New(1);
    PyTuple_SetItem(py_args, 0, static_cast<PyObject*>(py_det_model_handle));
    py_det_out_layer_handle = static_cast<void*>(PyObject_CallObject(static_cast<PyObject*>(py_get_model_output_layer), py_args));

    //  获取编译后的识别模型
    py_args = PyTuple_New(1);
    py_value = Py_BuildValue("s", "../ch_PP-OCRv3_rec_infer/inference.pdmodel");
    PyTuple_SetItem(py_args, 0, py_value);
    py_rec_model_handle = static_cast<void*>(PyObject_CallObject(static_cast<PyObject*>(py_get_compile_model), py_args));

    //  获取识别输出层
    py_args = PyTuple_New(1);
    PyTuple_SetItem(py_args, 0, static_cast<PyObject*>(py_rec_model_handle));
    py_rec_out_layer_handle = static_cast<void*>(PyObject_CallObject(static_cast<PyObject*>(py_get_model_output_layer), py_args));

    //  创建字典
    py_dict_handle = Py_BuildValue("s", "../dict.txt");

#else
    //  初始化快速模型
    py_ocr_init_fast   = static_cast<void*>(PyObject_GetAttrString(static_cast<PyObject*>(py_module), "ocr_init_fast"));
    py_ocr_fast_handle = static_cast<void*>(PyObject_CallObject(static_cast<PyObject*>(py_ocr_init_fast), nullptr));   //  得到OCR句柄

    //  初始化精确模型
    py_ocr_init_precise   = static_cast<void*>(PyObject_GetAttrString(static_cast<PyObject*>(py_module), "ocr_init_precise"));
    py_ocr_precise_handle = static_cast<void*>(PyObject_CallObject(static_cast<PyObject*>(py_ocr_init_precise), nullptr));   //  得到OCR句柄
#endif

    //  载入执行OCR本体
    py_ocr_ex = static_cast<void*>(PyObject_GetAttrString(static_cast<PyObject*>(py_module), "ocr_ex"));
}

//  只支持输入RGB888格式的数据
//  images (list[numpy.ndarray]): 图片数据，ndarray.shape 为 [H, W, C]
std::list<CPyOCR::SOCRdata> CPyOCR::OCR_Ex(void* buf, int w, int h, EModeType mode_type)
{
    //  定义返回值
    std::list<SOCRdata> re_list;

    //  检查
#if USE_OPENVINO
    if((py_get_compile_model      == nullptr) ||
       (py_get_model_output_layer == nullptr) ||
       (py_det_model_handle       == nullptr) ||
       (py_rec_model_handle       == nullptr) ||
       (py_det_out_layer_handle   == nullptr) ||
       (py_rec_out_layer_handle   == nullptr) ||
       (py_dict_handle            == nullptr)
      )
#else
    if((py_ocr_fast_handle == nullptr) || (py_ocr_precise_handle == nullptr))
#endif
    {
        return re_list;
    }

    //  构造输入数据
    npy_intp dims[3] = {h, w, 3};
    PyObject* py_value = PyArray_SimpleNewFromData(3, dims, NPY_UINT8, static_cast<unsigned char*>(buf));

    //  构造输入参数
#if USE_OPENVINO
    PyObject* py_args = PyTuple_New(6);
    PyTuple_SetItem(py_args, 0, py_value);                                                     //  第一个参数为图像数据
    PyTuple_SetItem(py_args, 1, static_cast<PyObject*>(py_det_model_handle));                  //  第二个参数为检测模型句柄
    PyTuple_SetItem(py_args, 2, static_cast<PyObject*>(py_det_out_layer_handle));              //  第三个参数为检测输出层句柄
    PyTuple_SetItem(py_args, 3, static_cast<PyObject*>(py_rec_model_handle));                  //  第四个参数为识别模型句柄
    PyTuple_SetItem(py_args, 4, static_cast<PyObject*>(py_rec_out_layer_handle));              //  第五个参数为识别输出层句柄
    PyTuple_SetItem(py_args, 5, static_cast<PyObject*>(py_dict_handle));                       //  第六个参数为字典句柄
#else
    PyObject* py_args = PyTuple_New(2);
    PyTuple_SetItem(py_args, 0, py_value);                                                     //  第一个参数为图像数据

    //  第二个参数为ocr句柄
    //  根据不同的模型选择
    if(mode_type == EModeType_Fast)
    {
        PyTuple_SetItem(py_args, 1, static_cast<PyObject*>(py_ocr_fast_handle));
    }
    else
    {
        PyTuple_SetItem(py_args, 1, static_cast<PyObject*>(py_ocr_precise_handle));
    }
#endif

    //  执行OCR
    PyObject* py_ret = PyObject_CallObject(static_cast<PyObject*>(py_ocr_ex), py_args);

    //  检查
    if(py_ret == nullptr)
    {
        printf("py_ret == nullptr\n");
        return re_list;
    }

    //  获取返回结果的数量
    long size = PyList_Size(py_ret);

    //  遍历全部结果
    long i=0;
    for(i=0;i<size;i++)
    {
        //  构造其中一组数据
        SOCRdata ocr_dat;

        //  获取当前list中的一列数据
        PyObject* cur_item = PyList_GetItem(py_ret, i);
        if(cur_item == nullptr)
        {
            printf("cur_item == nullptr\n");
            return re_list;
        }

#if USE_OPENVINO
        //  拿到字符串
        PyObject* item_text = PyList_GetItem(cur_item, 0);
        const char* str = PyUnicode_AsUTF8(item_text);
        if(str == nullptr)
        {
            printf("str == nullptr\n");
            return re_list;
        }
        ocr_dat.str = str;

        //  拿到信心值
        PyObject* item_conf = PyList_GetItem(cur_item, 1);
        double conf = PyFloat_AsDouble(item_conf);
        ocr_dat.confidence = conf;

        //  拿到当前列中的文本坐标值
        long x0 = PyLong_AsLong(PyList_GetItem(cur_item, 2));
        long y0 = PyLong_AsLong(PyList_GetItem(cur_item, 3));
        long x1 = PyLong_AsLong(PyList_GetItem(cur_item, 4));
        long y1 = PyLong_AsLong(PyList_GetItem(cur_item, 5));
        long x2 = PyLong_AsLong(PyList_GetItem(cur_item, 6));
        long y2 = PyLong_AsLong(PyList_GetItem(cur_item, 7));
        long x3 = PyLong_AsLong(PyList_GetItem(cur_item, 8));
        long y3 = PyLong_AsLong(PyList_GetItem(cur_item, 9));
        ocr_dat.postion[0] = x0;
        ocr_dat.postion[1] = y0;
        ocr_dat.postion[2] = x1;
        ocr_dat.postion[3] = y1;
        ocr_dat.postion[4] = x2;
        ocr_dat.postion[5] = y2;
        ocr_dat.postion[6] = x3;
        ocr_dat.postion[7] = y3;
#else
        //  拿到当前列中的text的字典的值
        PyObject* item_text_key = PyUnicode_FromString("text");
        PyObject* item_text = PyDict_GetItem(cur_item, item_text_key);

        //  将text转换为字符串
        const char* str = PyUnicode_AsUTF8(item_text);
        if(str == nullptr)
        {
            printf("str == nullptr\n");
            return re_list;
        }
        ocr_dat.str = str;

        //  拿到当前列中的信心值
        PyObject* item_conf_key = PyUnicode_FromString("confidence");
        PyObject* item_conf = PyDict_GetItem(cur_item, item_conf_key);

        //  将信心值转换为浮点
        double conf = PyFloat_AsDouble(item_conf);
        ocr_dat.confidence = conf;

        //  拿到当前列中的文本坐标值
        PyObject* item_postion_key = PyUnicode_FromString("text_box_position");
        PyObject* item_postion = PyDict_GetItem(cur_item, item_postion_key);

        //  将坐标值转换为整型
        PyObject* pos0 = PyList_GetItem(item_postion, 0);
        PyObject* pos1 = PyList_GetItem(item_postion, 1);
        PyObject* pos2 = PyList_GetItem(item_postion, 2);
        PyObject* pos3 = PyList_GetItem(item_postion, 3);
        long x0 = PyLong_AsLong(PyList_GetItem(pos0, 0));
        long y0 = PyLong_AsLong(PyList_GetItem(pos0, 1));
        long x1 = PyLong_AsLong(PyList_GetItem(pos1, 0));
        long y1 = PyLong_AsLong(PyList_GetItem(pos1, 1));
        long x2 = PyLong_AsLong(PyList_GetItem(pos2, 0));
        long y2 = PyLong_AsLong(PyList_GetItem(pos2, 1));
        long x3 = PyLong_AsLong(PyList_GetItem(pos3, 0));
        long y3 = PyLong_AsLong(PyList_GetItem(pos3, 1));
        ocr_dat.postion[0] = x0;
        ocr_dat.postion[1] = y0;
        ocr_dat.postion[2] = x1;
        ocr_dat.postion[3] = y1;
        ocr_dat.postion[4] = x2;
        ocr_dat.postion[5] = y2;
        ocr_dat.postion[6] = x3;
        ocr_dat.postion[7] = y3;

        //  释放资源
        Py_DecRef(pos3);
        Py_DecRef(pos2);
        Py_DecRef(pos1);
        Py_DecRef(pos0);
        Py_DecRef(item_postion);
        Py_DecRef(item_postion_key);
        Py_DecRef(item_conf);
        Py_DecRef(item_conf_key);
        //Py_DecRef(item_text);  //  由于使用了PyUnicode_AsUTF8方式，所以不能释放
        Py_DecRef(item_text_key);
#endif
        //  释放公共资源
        Py_DecRef(cur_item);

        //  插入当前该组数据
        re_list.insert(re_list.end(), ocr_dat);
    }

    //  释放资源
    Py_DecRef(py_ret);
    Py_DecRef(py_value);
    //Py_DecRef(py_args);    //  由于其中包含了模型句柄,所以不能释放

    //  返回全部数据
    return re_list;
}


