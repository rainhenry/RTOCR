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

#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    //  初始化UI界面
    //  设置原始图像和输出图像的默认颜色
    QImage img_origin(ui->label_origin_img->width(), ui->label_origin_img->height(), QImage::Format_RGB888);
    img_origin.fill(QColor(0, 0, 255));
    ui->label_origin_img->setPixmap(QPixmap::fromImage(img_origin));
    QImage img_output(ui->label_output_img->width(), ui->label_output_img->height(), QImage::Format_RGB888);
    img_output.fill(QColor(0, 255, 255));
    ui->label_output_img->setPixmap(QPixmap::fromImage(img_output));

    //  设置当前模式，并控制控件的显示
    cur_mode = EMode_Photo;
    ui->groupBox_photo->setEnabled(true);

    //  设置工作模式单选
    mode_grp.addButton(ui->radioButton_photo,  EMode_Photo);
    mode_grp.addButton(ui->radioButton_camera, EMode_Camera);
    mode_grp.addButton(ui->radioButton_video,  EMode_Video);

    //  连接工作模式单选信号
    connect(&mode_grp, SIGNAL(buttonClicked(int)), this, SLOT(slot_OnModeSelect(int)));

    //  设置OCR模型选择
    ocr_mode_grp.addButton(ui->radioButton_fast_mode,    CPyOCR::EModeType_Fast);
    ocr_mode_grp.addButton(ui->radioButton_precise_mode, CPyOCR::EModeType_Precise);

    //  连接OCR模型选择信号
    connect(&ocr_mode_grp, SIGNAL(buttonClicked(int)), this, SLOT(slot_OnOCRModelSelect(int)));

    //  禁用全部功能(因为OCR环境还未初始化)
    ui->groupBox_mode     ->setEnabled(false);
    ui->groupBox_photo    ->setEnabled(false);
    ui->groupBox_camera   ->setEnabled(false);
    ui->groupBox_video    ->setEnabled(false);
    ui->groupBox_mode_type->setEnabled(false);

    //  提示正在初始化OCR环境
    ui->textEdit_outtext->setText("Initializing OCR environment, please wait...");

    //  初始化相机变量
    m_camera = nullptr;
    m_viewfinder = nullptr;
    camera_list.clear();

    //  初始相机相关的UI
    ui->pushButton_cam_open ->setEnabled(false);
    ui->pushButton_cam_close->setEnabled(false);
    ui->checkBox_cam_rtocr  ->setEnabled(true);
    ui->pushButton_cam_ocr  ->setEnabled(false);
    ui->checkBox_cam_rtocr  ->setChecked(false);

    //  初始化视频播放相关的UI
    ui->pushButton_video_play  ->setEnabled(false);
    ui->pushButton_video_pause ->setEnabled(false);
    ui->pushButton_video_stop  ->setEnabled(false);
    ui->pushButton_video_ocr   ->setEnabled(false);

    //  视频播放
    m_mediaplayer = nullptr;

    //  视频播放状态
    video_st = EVideoSt_Stop;

    //  视频探针
    m_videoprobe = nullptr;

    //  初始化视频相关OCR标志
    single_video_ocr_flag     = false;
    continuous_video_ocr_flag = false;

    //  配置模型默认类型
    ocr_model_type = CPyOCR::EModeType_Fast;

    //  初始化OCR环境
    qRegisterMetaType<CQtOCR::SOCRout>("CQtOCR::SOCRout");
    connect(&ocr, &CQtOCR::send_ocr_result,            this, &MainWindow::slot_OnOCRFinish);
    connect(&ocr, &CQtOCR::send_ocr_environment_ready, this, &MainWindow::slot_OnOCREnvReady);
    ocr.Init();
    ocr.Configure(0.8, true);    //  配置默认的信心阈值与是否低阈值画图
    ocr.start();

}

MainWindow::~MainWindow()
{
    //  释放视频资源
    if(m_mediaplayer != nullptr)
    {
        m_mediaplayer->stop();
        delete m_mediaplayer;
        m_mediaplayer = nullptr;
    }

    //  释放视频探针
    if(m_videoprobe != nullptr)
    {
        delete m_videoprobe;
        m_videoprobe = nullptr;
    }

    //  释放相机变量
    if(m_viewfinder != nullptr)
    {
        delete m_viewfinder;
        m_viewfinder = nullptr;
    }
    if(m_camera != nullptr)
    {
        delete m_camera;
        m_camera = nullptr;
    }

    //  释放OCR资源
    ocr.Release();

    //  释放UI资源
    delete ui;
}

//  根据当前模式更新控件
void MainWindow::update_mode_UI(void)
{
    //  根据模式来控制UI控件
    //  当选中图片模式
    if(cur_mode == EMode_Photo)
    {
        ui->groupBox_photo ->setEnabled(true);
        ui->groupBox_camera->setEnabled(false);
        ui->groupBox_video ->setEnabled(false);
    }
    //  当选中摄像头模式
    else if(cur_mode == EMode_Camera)
    {
        ui->groupBox_photo ->setEnabled(false);
        ui->groupBox_camera->setEnabled(true);
        ui->groupBox_video ->setEnabled(false);
    }
    //  当选中视频文件模式
    else if(cur_mode == EMode_Video)
    {
        ui->groupBox_photo ->setEnabled(false);
        ui->groupBox_camera->setEnabled(false);
        ui->groupBox_video ->setEnabled(true);
    }
}

//  当OCR环境就绪
void MainWindow::slot_OnOCREnvReady(void)
{
    //  启用模式选择
    ui->groupBox_mode     ->setEnabled(true);
#if !USE_OPENVINO
    ui->groupBox_mode_type->setEnabled(true);
#endif

    //  设置初始状态
    cur_mode = EMode_Photo;
    update_mode_UI();
    ui->radioButton_photo ->setChecked(true);
    ui->radioButton_camera->setChecked(false);
    ui->radioButton_video ->setChecked(false);

    //  初始化缩放参数
    in_img_scale.press_x  = 0;
    in_img_scale.press_y  = 0;
    in_img_scale.x        = 0;
    in_img_scale.y        = 0;
    in_img_scale.factor   = 1.0;
    out_img_scale.press_x = 0;
    out_img_scale.press_y = 0;
    out_img_scale.x       = 0;
    out_img_scale.y       = 0;
    out_img_scale.factor  = 1.0;

    //  开启鼠标功能
    press_mouse_left_btn = false;
    setMouseTracking(true);
    centralWidget()->setMouseTracking(true);
    ui->label_origin_img->setMouseTracking(true);
    ui->label_output_img->setMouseTracking(true);

    //  提示环境就绪
    ui->textEdit_outtext  ->setText("Ready!!");
}


//  当完成一次OCR
void MainWindow::slot_OnOCRFinish(CQtOCR::SOCRout ocr_info)
{
    //  保存输出结果到图像副本
    out_img_cache = ocr_info.out_img;

    //  复位输出图像缩放参数
    out_img_scale.press_x = 0;
    out_img_scale.press_y = 0;
    out_img_scale.x       = 0;
    out_img_scale.y       = 0;
    out_img_scale.factor  = 1.0;

    //  绘制OCR输出的标记图像
    QImage img = ocr_info.out_img;
    img = img.scaled(ui->label_output_img->width(), ui->label_output_img->height(), Qt::KeepAspectRatio);
    ui->label_output_img->setPixmap(QPixmap::fromImage(img));

    //  将OCR结果输出到文本区
    QString out_text;

    //  显示运行时间，帧率
    double run_ms = ocr_info.run_time_ns;
    run_ms = run_ms / 1000000.0;
    double fps = ocr_info.run_time_ns;
    fps = 1000000000.0 / fps;
    out_text += QString::asprintf("run time: %0.3lf ms, FPS: %0.2lf at %dx%d type:%s\n",
                                  run_ms, fps, ocr_info.out_img.width(), ocr_info.out_img.height(),
                              #if USE_OPENVINO
                                  "OpenVINO");
                              #else
                                  (ocr_info.mode_type == CPyOCR::EModeType_Fast)?"fast":"precise");
                              #endif

    //  遍历每一条结果
    int count = 0;
    std::list<CPyOCR::SOCRdata> ocr_text_info = ocr_info.out_dat;
    while(ocr_text_info.size() > 0)
    {
        //  统计识别个数
        count++;

        //  获取当前信息
        CPyOCR::SOCRdata cur_dat = *(ocr_text_info.begin());

        //  计算信心值的去尾处理
        //  由于很多图片的识别的效果为0.99999，此时如果直接四舍五入结果就会变成1.0000
        //  这样很显然不够严谨，所以采用去尾的方法显示
        int confid_i = static_cast<int>(cur_dat.confidence * 10000.0);
        double confid_d = confid_i / 10000.0;

        //  显示信息
        out_text += QString::asprintf("[%3d] {%s} (%0.4lf)\n", count, cur_dat.str.c_str(), confid_d);

        //  完成当前信息的处理
        ocr_text_info.erase(ocr_text_info.begin());
    }

    //  提示统计信息
    out_text += QString::asprintf("Total OCR Count: %d\n", count);

    //  显示到控件
    ui->textEdit_outtext->setText(out_text);

    //  当为图片模式
    if(cur_mode == EMode_Photo)
    {
        //  打开模式UI
        ui->groupBox_mode->setEnabled(true);
    }

    //  根据模式来控制UI控件
    update_mode_UI();

    //  当选中视频文件模式，并且为播放状态，并且在实时OCR的模式下
    if((cur_mode == EMode_Video) && (video_st == EVideoSt_Play) && continuous_video_ocr_flag)
    {
        //  逐帧播放
        if(m_mediaplayer != nullptr) m_mediaplayer->play();
    }
}

//  模式单选槽
void MainWindow::slot_OnModeSelect(int mode)
{
    //  获取当前模式
    cur_mode = static_cast<EMode>(mode);

    //  根据模式来控制UI控件
    update_mode_UI();
}

//  选择OCR模型
void MainWindow::slot_OnOCRModelSelect(int type)
{
    ocr_model_type = static_cast<CPyOCR::EModeType>(type);
}


//  当单击浏览图片的按钮
void MainWindow::on_pushButton_photo_browse_clicked()
{
    QString str = QFileDialog::getOpenFileName(this, "Open Photo File", ".", "All file(*.*)");
    if(!str.isNull())
    {
        ui->lineEdit_photo_path->setText(str);
    }
}

//  单击打开图片文件
void MainWindow::on_pushButton_photo_open_clicked()
{
    QString file_name = ui->lineEdit_photo_path->text();

    if(!file_name.isNull())
    {
        //  尝试显示原始图片
        QImage img;
        img.load(file_name);

        //  当图片载入失败
        if(img.isNull())
        {
            ui->textEdit_outtext->setText("Photo opening failed!!");
            return;
        }

        //  保存原始输入图像副本
        in_img_cache = img;

        //  复位输入图像的缩放参数
        in_img_scale.press_x  = 0;
        in_img_scale.press_y  = 0;
        in_img_scale.x        = 0;
        in_img_scale.y        = 0;
        in_img_scale.factor   = 1.0;

        //  显示原始图片
        QImage img_show = img.scaled(ui->label_origin_img->width(), ui->label_origin_img->height(), Qt::KeepAspectRatio);
        ui->label_origin_img->setPixmap(QPixmap::fromImage(img_show));

        //  关闭按钮
        ui->groupBox_mode  ->setEnabled(false);
        ui->groupBox_photo ->setEnabled(false);
        ui->groupBox_camera->setEnabled(false);

        //  提示信息
        ui->textEdit_outtext->setText("Please wait...");

        //  发起执行OCR
        ocr.ExOCR(img, ocr_model_type);
    }
}

//  单击刷新相机设备列表
void MainWindow::on_pushButton_cam_refresh_clicked()
{
    //  清空原有信息
    ui->comboBox_cam_list->clear();
    camera_list.clear();

    //  获取相机设备列表
    camera_list = QCameraInfo::availableCameras();
    for(int i=0;i<camera_list.size();i++)
    {
        ui->comboBox_cam_list   ->addItem(camera_list.at(i).deviceName(), i);
    }

    //  当存在合法的相机设备
    if(camera_list.size() > 0)
    {
        ui->pushButton_cam_open ->setEnabled(true);
        ui->pushButton_cam_close->setEnabled(false);
        ui->pushButton_cam_ocr  ->setEnabled(false);
    }
}

//  单击打开相机
void MainWindow::on_pushButton_cam_open_clicked()
{
    //  设置标志
    single_video_ocr_flag     = false;
    continuous_video_ocr_flag = ui->checkBox_cam_rtocr->isChecked();

    //  初始化相机相关
    m_camera     = new QCamera(ui->comboBox_cam_list->currentText().toUtf8());    //  创建相机对象
    m_camera     ->setCaptureMode(QCamera::CaptureViewfinder);                    //  设置为取景器模式
    m_viewfinder = new QCameraViewfinder();                                       //  创建取景器
    m_camera     ->setViewfinder(m_viewfinder);                                   //  设置取景器

    //  检查视频探针
    if(m_videoprobe == nullptr)
    {
        //  创建视频帧探针
        m_videoprobe = new QVideoProbe;

        //  链接视频帧探针的信号
        connect(m_videoprobe, SIGNAL(videoFrameProbed(QVideoFrame)), this, SLOT(slot_OnVideoProbeFrame(QVideoFrame)));
    }

    //  配置探针的源数据
    m_videoprobe ->setSource(m_camera);

    //  打开相机
    m_camera->start();

    //  配置UI
    ui->pushButton_cam_open ->setEnabled(false);
    ui->pushButton_cam_close->setEnabled(true);
    ui->pushButton_cam_ocr  ->setEnabled(true);
    ui->groupBox_mode       ->setEnabled(false);
}

//  视频流探针槽
void MainWindow::slot_OnVideoProbeFrame(const QVideoFrame& frame)
{
    //  检查
    if(!frame.isValid()) return;

    //  复制一份当前帧的副本
    QVideoFrame cloneFrame(frame);
    cloneFrame.map(QAbstractVideoBuffer::ReadOnly);   //  采用只读方式映射

    //  构造输出帧图像
    QImage img(cloneFrame.width(), cloneFrame.height(), QImage::Format_RGB888);
    unsigned char* in_buf = cloneFrame.bits();        //  视频流原始数据帧缓冲区
    unsigned char* out_buf = img.bits();              //  视频流输出数据帧缓冲区

    //  根据源帧的格式进行转换
    //  这里只处理常见格式
    if(cloneFrame.pixelFormat()==QVideoFrame::Format_YV12)
    {
        int y=0;
        int x=0;
        for(y=0;y<cloneFrame.height();y++)
        {
            for(x=0;x<cloneFrame.width();x++)
            {
                unsigned char Y = static_cast<unsigned char>(in_buf[y*cloneFrame.width() + x]);
                unsigned char V = static_cast<unsigned char>(in_buf[cloneFrame.width() * cloneFrame.height() + (y/2)*(cloneFrame.width()/2) + x/2]);
                unsigned char U = static_cast<unsigned char>(in_buf[cloneFrame.width() * cloneFrame.height() + ((cloneFrame.width()/2) * (cloneFrame.height()/2)) + (y/2)*(cloneFrame.width()/2) + x/2]);

                double R = Y + 1.402 * (V-128);
                double G = Y - 0.344136 * (U-128) - 0.714136 * (V-128);
                double B = Y + 1.772 * (U-128);

                if(R >= 255) R = 255;
                if(R <= 0) R = 0;
                if(G >= 255) G = 255;
                if(G <= 0) G = 0;
                if(B >= 255) B = 255;
                if(B <= 0) B = 0;

                out_buf[y*img.bytesPerLine() + x*3 + 0] = static_cast<unsigned char>(R);
                out_buf[y*img.bytesPerLine() + x*3 + 1] = static_cast<unsigned char>(G);
                out_buf[y*img.bytesPerLine() + x*3 + 2] = static_cast<unsigned char>(B);
            }
        }
    }
    else if(cloneFrame.pixelFormat()==QVideoFrame::Format_YUV420P)
    {
        int y=0;
        int x=0;
        for(y=0;y<cloneFrame.height();y++)
        {
            for(x=0;x<cloneFrame.width();x++)
            {
                unsigned char Y = static_cast<unsigned char>(in_buf[y*cloneFrame.width() + x]);
                unsigned char U = static_cast<unsigned char>(in_buf[cloneFrame.width() * cloneFrame.height() + (y/2)*(cloneFrame.width()/2) + x/2]);
                unsigned char V = static_cast<unsigned char>(in_buf[cloneFrame.width() * cloneFrame.height() + ((cloneFrame.width()/2) * (cloneFrame.height()/2)) + (y/2)*(cloneFrame.width()/2) + x/2]);

                double R = Y + 1.402 * (V-128);
                double G = Y - 0.344136 * (U-128) - 0.714136 * (V-128);
                double B = Y + 1.772 * (U-128);

                if(R >= 255) R = 255;
                if(R <= 0) R = 0;
                if(G >= 255) G = 255;
                if(G <= 0) G = 0;
                if(B >= 255) B = 255;
                if(B <= 0) B = 0;

                out_buf[y*img.bytesPerLine() + x*3 + 0] = static_cast<unsigned char>(R);
                out_buf[y*img.bytesPerLine() + x*3 + 1] = static_cast<unsigned char>(G);
                out_buf[y*img.bytesPerLine() + x*3 + 2] = static_cast<unsigned char>(B);
            }
        }
    }
    else if(cloneFrame.pixelFormat()==QVideoFrame::Format_NV12)
    {
        int y=0;
        int x=0;
        for(y=0;y<cloneFrame.height();y++)
        {
            for(x=0;x<cloneFrame.width();x++)
            {
                unsigned char Y = static_cast<unsigned char>(in_buf[y*cloneFrame.bytesPerLine() + x]);
                unsigned char U = static_cast<unsigned char>(in_buf[cloneFrame.bytesPerLine() * cloneFrame.height() + (y/2)*(cloneFrame.bytesPerLine()) + (x&(~1)) + 0]);
                unsigned char V = static_cast<unsigned char>(in_buf[cloneFrame.bytesPerLine() * cloneFrame.height() + (y/2)*(cloneFrame.bytesPerLine()) + (x&(~1)) + 1]);

                double R = Y + 1.402 * (V-128);
                double G = Y - 0.344136 * (U-128) - 0.714136 * (V-128);
                double B = Y + 1.772 * (U-128);

                if(R >= 255) R = 255;
                if(R <= 0) R = 0;
                if(G >= 255) G = 255;
                if(G <= 0) G = 0;
                if(B >= 255) B = 255;
                if(B <= 0) B = 0;

                out_buf[y*img.bytesPerLine() + x*3 + 0] = static_cast<unsigned char>(R);
                out_buf[y*img.bytesPerLine() + x*3 + 1] = static_cast<unsigned char>(G);
                out_buf[y*img.bytesPerLine() + x*3 + 2] = static_cast<unsigned char>(B);
            }
        }
    }
    else if(cloneFrame.pixelFormat()==QVideoFrame::Format_RGB32)
    {
        int y=0;
        int x=0;
        for(y=0;y<cloneFrame.height();y++)
        {
            for(x=0;x<cloneFrame.width();x++)
            {
                unsigned char R = static_cast<unsigned char>(in_buf[y*cloneFrame.bytesPerLine() + x*4 + 2]);
                unsigned char G = static_cast<unsigned char>(in_buf[y*cloneFrame.bytesPerLine() + x*4 + 1]);
                unsigned char B = static_cast<unsigned char>(in_buf[y*cloneFrame.bytesPerLine() + x*4 + 0]);

                out_buf[y*img.bytesPerLine() + x*3 + 0] = static_cast<unsigned char>(R);
                out_buf[y*img.bytesPerLine() + x*3 + 1] = static_cast<unsigned char>(G);
                out_buf[y*img.bytesPerLine() + x*3 + 2] = static_cast<unsigned char>(B);
            }
        }
    }
    else
    {
        qDebug("Unsupport in_fmt = %d w=%d, h=%d, bl=%d", cloneFrame.pixelFormat(), cloneFrame.width(), cloneFrame.height(), cloneFrame.bytesPerLine());
    }

    //  保存原始输入图像副本
    in_img_cache = img;

    //  复位输入图像的缩放参数
    in_img_scale.press_x  = 0;
    in_img_scale.press_y  = 0;
    in_img_scale.x        = 0;
    in_img_scale.y        = 0;
    in_img_scale.factor   = 1.0;

    //  当需要执行OCR的时候
    if(single_video_ocr_flag | continuous_video_ocr_flag)
    {
        //  发起执行OCR
        ocr.ExOCR(img, ocr_model_type);

        //  当选中视频文件模式，并且为播放状态,并且为实时OCR
        if((cur_mode == EMode_Video) && (video_st == EVideoSt_Play) && continuous_video_ocr_flag)
        {
            //  逐帧播放
            if(m_mediaplayer != nullptr) m_mediaplayer->pause();
        }

        //  当本次为单次执行
        if(single_video_ocr_flag)
        {
            single_video_ocr_flag = false;
        }
    }

    //  根据输出控件的尺寸进行缩放
    img = img.scaled(ui->label_origin_img->width(), ui->label_origin_img->height(), Qt::KeepAspectRatio);

    //  显示到控件上
    ui->label_origin_img->setPixmap(QPixmap::fromImage(img));

    //  取消映射
    cloneFrame.unmap();
}

//  单击关闭相机
void MainWindow::on_pushButton_cam_close_clicked()
{
    //  关闭相机
    m_camera->stop();

    //  释放资源
    delete m_videoprobe;
    m_videoprobe = nullptr;
    delete m_viewfinder;
    m_viewfinder = nullptr;
    delete m_camera;
    m_camera     = nullptr;

    //  设置标志
    single_video_ocr_flag     = false;
    continuous_video_ocr_flag = ui->checkBox_cam_rtocr->isChecked();

    //  设置UI
    ui->pushButton_cam_open ->setEnabled(true);
    ui->pushButton_cam_close->setEnabled(false);
    ui->pushButton_cam_ocr  ->setEnabled(false);
    ui->groupBox_mode       ->setEnabled(true);

    //  清除图像缓存
    QImage null_img;
    in_img_cache = null_img;

    //  设置原始图像和输出图像的默认颜色
    QImage img_origin(ui->label_origin_img->width(), ui->label_origin_img->height(), QImage::Format_RGB888);
    img_origin.fill(QColor(0, 0, 255));
    ui->label_origin_img->setPixmap(QPixmap::fromImage(img_origin));
}

//  相机模式下，单次执行OCR
void MainWindow::on_pushButton_cam_ocr_clicked()
{
    single_video_ocr_flag = true;
}

//  单击相机模式下的实时OCR
void MainWindow::on_checkBox_cam_rtocr_clicked(bool checked)
{
    //  设置标志
    continuous_video_ocr_flag = checked;

    //  控制另一个实时OCR
    ui->checkBox_video_rtocr->setChecked(checked);
}

//  单击视频文件浏览按钮
void MainWindow::on_pushButton_video_browse_clicked()
{
    QString str = QFileDialog::getOpenFileName(this, "Open Video File", ".", "All file(*.*)");
    if(!str.isNull())
    {
        ui->lineEdit_video_path->setText(str);
    }
}

//  单击载入视频
void MainWindow::on_pushButton_video_load_clicked()
{
    //  检查路径
    if(ui->lineEdit_video_path->text().isNull()) return;

    //  释放之前的资源
    if(m_mediaplayer != nullptr)
    {
        m_mediaplayer->stop();
        delete m_mediaplayer;
        m_mediaplayer = nullptr;
    }

    //  创建视频播放
    m_mediaplayer = new QMediaPlayer(this);

    //  检查视频探针
    if(m_videoprobe == nullptr)
    {
        //  创建视频帧探针
        m_videoprobe = new QVideoProbe;

        //  链接视频帧探针的信号
        connect(m_videoprobe, SIGNAL(videoFrameProbed(QVideoFrame)), this, SLOT(slot_OnVideoProbeFrame(QVideoFrame)));
    }

    //  配置探针的源数据
    m_videoprobe ->setSource(m_mediaplayer);

    //  载入视频文件
    m_mediaplayer->setMedia(QUrl::fromLocalFile(ui->lineEdit_video_path->text()));

    //  载入后，先暂停
    m_mediaplayer->pause();
    video_st = EVideoSt_Pause;

    //  使能控件
    ui->pushButton_video_play  ->setEnabled(true);
    ui->pushButton_video_pause ->setEnabled(true);
    ui->pushButton_video_stop  ->setEnabled(true);
    ui->pushButton_video_ocr   ->setEnabled(true);
}

//  视频播放按钮
void MainWindow::on_pushButton_video_play_clicked()
{
    //  控制视频
    m_mediaplayer->play();
    video_st = EVideoSt_Play;

    //  配置UI
    ui->groupBox_mode->setEnabled(false);
}

//  视频暂停按钮
void MainWindow::on_pushButton_video_pause_clicked()
{
    m_mediaplayer->pause();
    video_st = EVideoSt_Pause;
}

//  视频停止按钮
void MainWindow::on_pushButton_video_stop_clicked()
{
    //  控制视频
    m_mediaplayer->stop();
    m_mediaplayer->pause();
    video_st = EVideoSt_Stop;

    //  配置UI
    ui->groupBox_mode->setEnabled(true);
}

//  视频中执行一次OCR
void MainWindow::on_pushButton_video_ocr_clicked()
{
    //  当为暂停
    if(video_st == EVideoSt_Pause)
    {
        //  发起执行OCR
        ocr.ExOCR(in_img_cache, ocr_model_type);

        //  复位输入图像的缩放参数
        in_img_scale.press_x  = 0;
        in_img_scale.press_y  = 0;
        in_img_scale.x        = 0;
        in_img_scale.y        = 0;
        in_img_scale.factor   = 1.0;
    }
    //  当为播放
    else if(video_st == EVideoSt_Play)
    {
        //  开启一次执行
        single_video_ocr_flag = true;
    }
}

//  视频文件的实时OCR
void MainWindow::on_checkBox_video_rtocr_clicked(bool checked)
{
    //  更新状态
    continuous_video_ocr_flag = checked;

    //  当取消实时OCR
    if(!checked)
    {
        //  当为播放
        if(video_st == EVideoSt_Play)
        {
            //  恢复正常播放
            if(m_mediaplayer != nullptr) m_mediaplayer->play();
        }
    }

    //  控制另一个实时OCR
    ui->checkBox_cam_rtocr->setChecked(checked);
}

//  鼠标按下处理
void MainWindow::mousePressEvent(QMouseEvent* event)
{
    if(Qt::LeftButton == event->button())
    {
        //  记录按下状态
        press_mouse_left_btn = true;

        //  在输入图像控件上的处理
        QRect rect_in_img = ui->label_origin_img->geometry();              //  获取该图像控件的位置信息
        bool on_in_img = rect_in_img.contains(event->x(), event->y());     //  判断鼠标是否在该控件内
        int in_img_pos_x = event->x() - rect_in_img.x();                   //  计算相对坐标x
        int in_img_pos_y = event->y() - rect_in_img.y();                   //  计算相对坐标y

        //  当在该图像中，并且图像可用
        if(on_in_img && (!in_img_cache.isNull()))
        {
            //  记录按下时刻的坐标
            in_img_scale.press_x = in_img_pos_x;
            in_img_scale.press_y = in_img_pos_y;
        }

        //  在输出图像控件上的处理
        QRect rect_out_img = ui->label_output_img->geometry();             //  获取该图像控件的位置信息
        bool on_out_img = rect_out_img.contains(event->x(), event->y());   //  判断鼠标是否在该控件内
        int out_img_pos_x = event->x() - rect_out_img.x();                 //  计算相对坐标x
        int out_img_pos_y = event->y() - rect_out_img.y();                 //  计算相对坐标y

        //  当在该图像中，并且图像可用
        if(on_out_img && (!out_img_cache.isNull()))
        {
            //  记录按下时刻的坐标
            out_img_scale.press_x = out_img_pos_x;
            out_img_scale.press_y = out_img_pos_y;
        }
    }
}

//  鼠标抬起处理
void MainWindow::mouseReleaseEvent(QMouseEvent* event)
{
    if(Qt::LeftButton == event->button())
    {
        //  释放按下状态
        press_mouse_left_btn = false;
    }
}

//  鼠标双击处理
void MainWindow::mouseDoubleClickEvent(QMouseEvent* event)
{
    //  先处理输出
    //  当OCR处理忙
    if(ocr.GetStatus() == CQtOCR::EOCRSt_Busy) return;

    //  当处于摄像机模式，并且相机已经打开,并且实时OCR中
    if((cur_mode == EMode_Camera) && (m_camera != nullptr) && continuous_video_ocr_flag) return;

    //  当处于视频播放模式，并播放中，并实时OCR中
    if((cur_mode == EMode_Video) && (video_st == EVideoSt_Play) && continuous_video_ocr_flag) return;

    //  复位缩放参数
    out_img_scale.press_x = 0;
    out_img_scale.press_y = 0;
    out_img_scale.x       = 0;
    out_img_scale.y       = 0;
    out_img_scale.factor  = 1.0;

    //  在输出图像控件上的处理
    QRect rect_out_img = ui->label_output_img->geometry();             //  获取该图像控件的位置信息
    bool on_out_img = rect_out_img.contains(event->x(), event->y());   //  判断鼠标是否在该控件内

    //  在输出图片上
    if(on_out_img)
    {
        //  复位显示
        QImage img = out_img_cache;
        img = img.scaled(ui->label_output_img->width(), ui->label_output_img->height(), Qt::KeepAspectRatio);
        ui->label_output_img->setPixmap(QPixmap::fromImage(img));
    }

    //  后处理输入
    //  当处于摄像机模式,并且相机打开了
    if((cur_mode == EMode_Camera) && (m_camera != nullptr)) return;

    //  当处于视频播放模式，并播放中
    if((cur_mode == EMode_Video) && (video_st == EVideoSt_Play)) return;

    //  复位缩放参数
    in_img_scale.press_x  = 0;
    in_img_scale.press_y  = 0;
    in_img_scale.x        = 0;
    in_img_scale.y        = 0;
    in_img_scale.factor   = 1.0;

    //  在输入图像控件上的处理
    QRect rect_in_img = ui->label_origin_img->geometry();              //  获取该图像控件的位置信息
    bool on_in_img = rect_in_img.contains(event->x(), event->y());     //  判断鼠标是否在该控件内

    //  在输入图片上
    if(on_in_img)
    {
        //  复位显示
        QImage img = in_img_cache;
        img = img.scaled(ui->label_origin_img->width(), ui->label_origin_img->height(), Qt::KeepAspectRatio);
        ui->label_origin_img->setPixmap(QPixmap::fromImage(img));
    }

}

//  鼠标移动处理
void MainWindow::mouseMoveEvent(QMouseEvent* event)
{
    //  先处理输出
    //  当OCR处理忙
    if(ocr.GetStatus() == CQtOCR::EOCRSt_Busy) return;

    //  当处于摄像机模式,并且相机已经打开，并且实时OCR中
    if((cur_mode == EMode_Camera) && (m_camera != nullptr) && continuous_video_ocr_flag) return;

    //  当处于视频播放模式，并播放中，并实时OCR中
    if((cur_mode == EMode_Video) && (video_st == EVideoSt_Play) && continuous_video_ocr_flag) return;

    //  在输出图像控件上的处理
    QRect rect_out_img = ui->label_output_img->geometry();             //  获取该图像控件的位置信息
    bool on_out_img = rect_out_img.contains(event->x(), event->y());   //  判断鼠标是否在该控件内
    int out_img_pos_x = event->x() - rect_out_img.x();                 //  计算相对坐标x
    int out_img_pos_y = event->y() - rect_out_img.y();                 //  计算相对坐标y

    //  当在该图像中，并且图像可用, 并且为按下
    if(on_out_img && (!out_img_cache.isNull()) && press_mouse_left_btn)
    {
        //  计算新位置
        int new_x = (out_img_scale.press_x - out_img_pos_x) * (out_img_cache.width()  / ui->label_output_img->width())  / static_cast<int>(out_img_scale.factor);
        int new_y = (out_img_scale.press_y - out_img_pos_y) * (out_img_cache.height() / ui->label_output_img->height()) / static_cast<int>(out_img_scale.factor);

        //  保存
        out_img_scale.x += new_x;
        out_img_scale.y += new_y;

        //  限制
        int min_x = -(out_img_cache.width()  / 3);
        int min_y = -(out_img_cache.height() / 3);
        if(out_img_scale.x < min_x) out_img_scale.x = min_x;
        if(out_img_scale.y < min_y) out_img_scale.y = min_y;
        int max_x = out_img_cache.width()  + out_img_cache.width() / 3;
        int max_y = out_img_cache.height() + out_img_cache.height() / 3;
        if(out_img_scale.x > max_x) out_img_scale.x = max_x;
        if(out_img_scale.y > max_y) out_img_scale.y = max_y;

        //  执行
        QImage img = out_img_cache;
        img = img.copy(out_img_scale.x, out_img_scale.y, static_cast<int>(img.width() / out_img_scale.factor), static_cast<int>(img.height() / out_img_scale.factor));
        img = img.scaled(ui->label_output_img->width(), ui->label_output_img->height(), Qt::KeepAspectRatio);
        ui->label_output_img->setPixmap(QPixmap::fromImage(img));

        //  更新按下时刻的坐标
        out_img_scale.press_x = out_img_pos_x;
        out_img_scale.press_y = out_img_pos_y;
    }

    //  后处理输入
    //  当处于摄像机模式,并且相机打开了
    if((cur_mode == EMode_Camera) && (m_camera != nullptr)) return;

    //  当处于视频播放模式，并播放中
    if((cur_mode == EMode_Video) && (video_st == EVideoSt_Play)) return;

    //  在输入图像控件上的处理
    QRect rect_in_img = ui->label_origin_img->geometry();              //  获取该图像控件的位置信息
    bool on_in_img = rect_in_img.contains(event->x(), event->y());     //  判断鼠标是否在该控件内
    int in_img_pos_x = event->x() - rect_in_img.x();                   //  计算相对坐标x
    int in_img_pos_y = event->y() - rect_in_img.y();                   //  计算相对坐标y

    //  当在该图像中，并且图像可用, 并且为按下
    if(on_in_img && (!in_img_cache.isNull()) && press_mouse_left_btn)
    {
        //  计算新位置
        int new_x = (in_img_scale.press_x - in_img_pos_x) * (in_img_cache.width()  / ui->label_origin_img->width())  / static_cast<int>(in_img_scale.factor);
        int new_y = (in_img_scale.press_y - in_img_pos_y) * (in_img_cache.height() / ui->label_origin_img->height()) / static_cast<int>(in_img_scale.factor);

        //  保存
        in_img_scale.x += new_x;
        in_img_scale.y += new_y;

        //  限制
        int min_x = -(in_img_cache.width()  / 3);
        int min_y = -(in_img_cache.height() / 3);
        if(in_img_scale.x < min_x) in_img_scale.x = min_x;
        if(in_img_scale.y < min_y) in_img_scale.y = min_y;
        int max_x = in_img_cache.width()  + in_img_cache.width() / 3;
        int max_y = in_img_cache.height() + in_img_cache.height() / 3;
        if(in_img_scale.x > max_x) in_img_scale.x = max_x;
        if(in_img_scale.y > max_y) in_img_scale.y = max_y;

        //  执行
        QImage img = in_img_cache;
        img = img.copy(in_img_scale.x, in_img_scale.y, static_cast<int>(img.width() / in_img_scale.factor), static_cast<int>(img.height() / in_img_scale.factor));
        img = img.scaled(ui->label_origin_img->width(), ui->label_origin_img->height(), Qt::KeepAspectRatio);
        ui->label_origin_img->setPixmap(QPixmap::fromImage(img));

        //  更新按下时刻的坐标
        in_img_scale.press_x = in_img_pos_x;
        in_img_scale.press_y = in_img_pos_y;
    }

}

//  鼠标滚轮处理
void MainWindow::wheelEvent(QWheelEvent* event)
{
    //  先处理输出
    //  当OCR处理忙
    if(ocr.GetStatus() == CQtOCR::EOCRSt_Busy) return;

    //  当处于摄像机模式,并且相机已经打开，并且实时OCR中
    if((cur_mode == EMode_Camera) && (m_camera != nullptr) && continuous_video_ocr_flag) return;

    //  当处于视频播放模式，并播放中，并实时OCR中
    if((cur_mode == EMode_Video) && (video_st == EVideoSt_Play) && continuous_video_ocr_flag) return;

    //  在输出图像控件上的处理
    QRect rect_out_img = ui->label_output_img->geometry();             //  获取该图像控件的位置信息
    bool on_out_img = rect_out_img.contains(event->x(), event->y());   //  判断鼠标是否在该控件内

    //  当在该图像中，并且图像可用
    if(on_out_img && (!out_img_cache.isNull()))
    {
        //  当放大
        if(event->delta() > 0)
        {
            //  在范围内
            if(out_img_scale.factor < 10.0)
            {
                out_img_scale.factor += 0.5;
            }
            //  超出放大范围
            else
            {
                out_img_scale.factor = 10.0;
            }
        }
        //  当缩小
        else
        {
            //  在范围内
            if(out_img_scale.factor > 1.0)
            {
                out_img_scale.factor -= 0.5;
            }
            //  超出缩小范围
            else
            {
                out_img_scale.factor = 1.0;
            }
        }

        //  执行
        QImage img = out_img_cache;
        img = img.copy(out_img_scale.x, out_img_scale.y, static_cast<int>(img.width() / out_img_scale.factor), static_cast<int>(img.height() / out_img_scale.factor));
        img = img.scaled(ui->label_output_img->width(), ui->label_output_img->height(), Qt::KeepAspectRatio);
        ui->label_output_img->setPixmap(QPixmap::fromImage(img));
    }

    //  后处理输入
    //  当处于摄像机模式,并且相机打开了
    if((cur_mode == EMode_Camera) && (m_camera != nullptr)) return;

    //  当处于视频播放模式，并播放中
    if((cur_mode == EMode_Video) && (video_st == EVideoSt_Play)) return;

    //  在输入图像控件上的处理
    QRect rect_in_img = ui->label_origin_img->geometry();              //  获取该图像控件的位置信息
    bool on_in_img = rect_in_img.contains(event->x(), event->y());     //  判断鼠标是否在该控件内

    //  当在该图像中，并且图像可用
    if(on_in_img && (!in_img_cache.isNull()))
    {
        //  当放大
        if(event->delta() > 0)
        {
            //  在范围内
            if(in_img_scale.factor < 10.0)
            {
                in_img_scale.factor += 0.5;
            }
            //  超出放大范围
            else
            {
                in_img_scale.factor = 10.0;
            }
        }
        //  当缩小
        else
        {
            //  在范围内
            if(in_img_scale.factor > 1.0)
            {
                in_img_scale.factor -= 0.5;
            }
            //  超出缩小范围
            else
            {
                in_img_scale.factor = 1.0;
            }
        }

        //  执行
        QImage img = in_img_cache;
        img = img.copy(in_img_scale.x, in_img_scale.y, static_cast<int>(img.width() / in_img_scale.factor), static_cast<int>(img.height() / in_img_scale.factor));
        img = img.scaled(ui->label_origin_img->width(), ui->label_origin_img->height(), Qt::KeepAspectRatio);
        ui->label_origin_img->setPixmap(QPixmap::fromImage(img));
    }

}



