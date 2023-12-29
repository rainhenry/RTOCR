/*************************************************************

    程序名称:主窗体
    程序版本:REV 0.1
    创建日期:20231224
    设计编写:rainhenry
    作者邮箱:rainhenry@savelife-tech.com
    版    权:SAVELIFE Technology of Yunnan Co., LTD
    开源协议:GPL

    版本修订
        REV 0.1   20231224      rainhenry    创建文档

*************************************************************/

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

//  包含头文件
#include <QMainWindow>

#include <QList>
#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QButtonGroup>
#include <QFileDialog>
#include <QMouseEvent>
#include <QWheelEvent>

#include <QCamera>
#include <QCameraInfo>
#include <QCameraViewfinder>
#include <QMediaPlayer>
#include <QVideoProbe>

#include "CQtOCR.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    //  模式定义
    typedef enum
    {
        EMode_Photo = 0,
        EMode_Camera,
        EMode_Video,
    }EMode;

    //  视频的播放状态
    typedef enum
    {
        EVideoSt_Stop = 0,
        EVideoSt_Play,
        EVideoSt_Pause,
    }EVideoSt;

    //  缩放参数
    typedef struct
    {
        int press_x;
        int press_y;
        int x;
        int y;
        double factor;
    }SScaleParam;

private:
    //  模式单选组
    QButtonGroup mode_grp;

    //  当前的工作模式
    EMode cur_mode;

    //  根据当前模式更新控件
    void update_mode_UI(void);

    //  OCR对象
    CQtOCR ocr;

    //  OCR模式单选组
    QButtonGroup ocr_mode_grp;

    //  模型的类型
    CPyOCR::EModeType ocr_model_type;

    //  输出图像的副本
    QImage out_img_cache;

    //  输入图像的副本
    QImage in_img_cache;

    //  相机相关
    QCamera* m_camera;                     //  相机对象
    QCameraViewfinder* m_viewfinder;       //  取景器
    QList<QCameraInfo> camera_list;        //  相机设备列表

    //  视频播放
    QMediaPlayer* m_mediaplayer;

    //  视频播放状态
    EVideoSt video_st;

    //  视频探针
    QVideoProbe* m_videoprobe;

    //  单次OCR的标志
    bool single_video_ocr_flag;

    //  连续OCR的标志
    bool continuous_video_ocr_flag;

    //  鼠标左键按下的状态
    bool press_mouse_left_btn;
    int press_mouse_pos_x;
    int press_mouse_pos_y;

    //  缩放参数
    SScaleParam in_img_scale;
    SScaleParam out_img_scale;

private slots:
    //  模式单选槽
    void slot_OnModeSelect(int mode);

    //  当OCR环境就绪
    void slot_OnOCREnvReady(void);

    //  当完成一次OCR
    void slot_OnOCRFinish(CQtOCR::SOCRout ocr_info);

    //  选择OCR模型
    void slot_OnOCRModelSelect(int type);

    //  视频流探针槽
    void slot_OnVideoProbeFrame(const QVideoFrame& frame);

    //  当单击浏览图片的按钮
    void on_pushButton_photo_browse_clicked();

    //  单击打开图片文件
    void on_pushButton_photo_open_clicked();

    //  单击刷新相机设备列表
    void on_pushButton_cam_refresh_clicked();

    //  单击打开相机
    void on_pushButton_cam_open_clicked();

    //  单击关闭相机
    void on_pushButton_cam_close_clicked();

    //  相机模式下，单次执行OCR
    void on_pushButton_cam_ocr_clicked();

    //  单击相机模式下的实时OCR
    void on_checkBox_cam_rtocr_clicked(bool checked);

    //  单击视频文件浏览按钮
    void on_pushButton_video_browse_clicked();

    //  单击载入视频
    void on_pushButton_video_load_clicked();

    //  视频播放按钮
    void on_pushButton_video_play_clicked();

    //  视频暂停按钮
    void on_pushButton_video_pause_clicked();

    //  视频停止按钮
    void on_pushButton_video_stop_clicked();

    //  视频中执行一次OCR
    void on_pushButton_video_ocr_clicked();

    //  视频文件的实时OCR
    void on_checkBox_video_rtocr_clicked(bool checked);

protected:
    //  鼠标相关
    void mousePressEvent(QMouseEvent* event);          //  按下
    void mouseReleaseEvent(QMouseEvent* event);        //  抬起
    void mouseDoubleClickEvent(QMouseEvent* event);    //  双击
    void mouseMoveEvent(QMouseEvent* event);           //  移动
    void wheelEvent(QWheelEvent* event);               //  滚轮

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;
};
#endif // MAINWINDOW_H
