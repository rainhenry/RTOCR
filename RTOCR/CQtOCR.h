/*************************************************************

    程序名称:基于Qt线程类的OCR类
    程序版本:REV 0.1
    创建日期:20231224
    设计编写:rainhenry
    作者邮箱:rainhenry@savelife-tech.com
    版    权:SAVELIFE Technology of Yunnan Co., LTD
    开源协议:GPL

    版本修订
        REV 0.1   20231224      rainhenry    创建文档

*************************************************************/
//------------------------------------------------------------
//  重定义保护
#ifndef __CQTOCR_H__
#define __CQTOCR_H__

//------------------------------------------------------------
//  包含头文件
#include <QVariant>
#include <QThread>
#include <QImage>
#include <QPainter>
#include <QSemaphore>
#include <QMutex>
#include <QElapsedTimer>

#include "CPyOCR.h"

//------------------------------------------------------------
//  类定义
class CQtOCR: public QThread
{
    Q_OBJECT

public:
    //  状态枚举
    typedef enum
    {
        EOCRSt_Ready = 0,
        EOCRSt_Busy,
        EOCRSt_Error
    }EOCRSt;

    //  命令枚举
    typedef enum
    {
        EOCRcmd_Null = 0,
        EOCRcmd_ExOCR,
        EOCRcmd_Release,
    }EOCRcmd;

    //  输出结构定义
    typedef struct
    {
        QImage out_img;                          //  标记完内容的输出图像
        std::list<CPyOCR::SOCRdata> out_dat;     //  OCR的结果
        qint64 run_time_ns;                      //  本次执行时间，单位纳秒
        CPyOCR::EModeType mode_type;             //  所用的模型类型
    }SOCRout;

    //  构造和析构函数
    CQtOCR();
    ~CQtOCR() override;

    //  线程运行本体
    void run() override;

    //  初始化
    void Init(void);

    //  配置
    void Configure(double th,       //  信心阈值
                   bool low_flag    //  低阈值是否画图标记
                  );

    //  执行一次OCR
    void ExOCR(QImage origin, CPyOCR::EModeType type);

    //  得到当前状态
    EOCRSt GetStatus(void);

    //  释放资源
    void Release(void);

signals:
    //  OCR环境就绪
    void send_ocr_environment_ready(void);

    //  当完成一次OCR发出的信号
    void send_ocr_result(CQtOCR::SOCRout ocr_info);

private:
    //  内部私有变量
    QSemaphore sem_cmd;           //  命令信号量
    QMutex cmd_mutex;             //  命令互斥量
    QImage in_img_dat;            //  输入的图像数据
    EOCRSt cur_st;                //  当前状态
    EOCRcmd cur_cmd;              //  当前命令
    CPyOCR* py_ocr;               //  Python的OCR对象
    double confid_th;             //  信心阈值
    bool confid_low_flag;         //  低于阈值是否画图
    CPyOCR::EModeType mode_type;  //  模型的类型
};

//------------------------------------------------------------
#endif  //  __CQTOCR_H__



