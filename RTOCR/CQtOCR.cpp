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

//  包含头文件
#include "CQtOCR.h"

//  构造函数
CQtOCR::CQtOCR():
    sem_cmd(0)
{
    //  初始化私有数据
    cur_st = EOCRSt_Ready;
    cur_cmd = EOCRcmd_Null;
    confid_th = 0.8;
    confid_low_flag = true;

    //  创建Python的OCR对象
    py_ocr = new CPyOCR();
}

//  析构函数
CQtOCR::~CQtOCR()
{
    Release();   //  通知线程退出
    this->wait(500);

    delete py_ocr;
}

//  初始化
void CQtOCR::Init(void)
{
    //  暂时没有
}

//  配置
void CQtOCR::Configure(double th,       //  信心阈值
                       bool low_flag    //  低阈值是否画图标记
                      )
{
    cmd_mutex.lock();
    this->confid_th = th;
    this->confid_low_flag = low_flag;
    cmd_mutex.unlock();
}

void CQtOCR::run()
{
    //  初始化python环境
    py_ocr->Init();

    //  通知运行环境就绪
    emit send_ocr_environment_ready();

    //  现成主循环
    while(1)
    {
        //  获取信号量
        sem_cmd.acquire();

        //  获取当前命令和数据
        cmd_mutex.lock();
        EOCRcmd now_cmd = this->cur_cmd;
        QImage now_img = this->in_img_dat;
        double now_confid_th = this->confid_th;
        bool now_low_flag = this->confid_low_flag;
        CPyOCR::EModeType now_type = this->mode_type;
        cmd_mutex.unlock();

        //  当为空命令
        if(now_cmd == EOCRcmd_Null)
        {
            //  释放CPU
            QThread::sleep(1);
        }
        //  当为退出命令
        else if(now_cmd == EOCRcmd_Release)
        {
            //py_ocr->Release();
            return;
        }
        //  当为OCR命令
        else if(now_cmd == EOCRcmd_ExOCR)
        {
            //  设置忙
            cmd_mutex.lock();
            this->cur_st = EOCRSt_Busy;
            cmd_mutex.unlock();

            //  转换图像格式为RGB888
            now_img.convertTo(QImage::Format_RGB888);

            //  去掉图像中每一行后面的多余对齐内存空间
            unsigned char* pimg = now_img.bits();
            unsigned char* pimg_buf = new unsigned char[now_img.width() * now_img.height() * 3];
            int y=0;
            for(y=0;y<now_img.height();y++)
            {
                memcpy(pimg_buf + y*now_img.width()*3,
                       pimg + y*now_img.bytesPerLine(),
                       static_cast<size_t>(now_img.width()*3)
                      );
            }

            //  执行OCR
            QElapsedTimer run_time;
            run_time.start();
            std::list<CPyOCR::SOCRdata> ocr_info = py_ocr->OCR_Ex(pimg_buf, now_img.width(), now_img.height(), now_type);
            std::list<CPyOCR::SOCRdata> ocr_buf_info = ocr_info;  //  保存副本，用于输出给用户
            qint64 run_time_ns = run_time.nsecsElapsed();

            //  释放缓存资源
            delete [] pimg_buf;

            //  创建画板
            QPainter* ppainter = new QPainter();
            ppainter->begin(&now_img);
            ppainter->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);    //  抗锯齿

            //  将OCR识别到的结果标记 
            while(ocr_info.size()>0)
            {
                //  获取当前条目数据
                CPyOCR::SOCRdata cur_info;
                cur_info = *(ocr_info.begin());

                //  当达到信心值
                if(cur_info.confidence >= now_confid_th)
                {
                    ppainter->setPen(QPen(QColor(255,0,0), 5, Qt::SolidLine));
                    ppainter->drawLine(static_cast<int>(cur_info.postion[0]), static_cast<int>(cur_info.postion[1]), 
                                       static_cast<int>(cur_info.postion[2]), static_cast<int>(cur_info.postion[3]));
                    ppainter->drawLine(static_cast<int>(cur_info.postion[2]), static_cast<int>(cur_info.postion[3]), 
                                       static_cast<int>(cur_info.postion[4]), static_cast<int>(cur_info.postion[5]));
                    ppainter->drawLine(static_cast<int>(cur_info.postion[4]), static_cast<int>(cur_info.postion[5]), 
                                       static_cast<int>(cur_info.postion[6]), static_cast<int>(cur_info.postion[7]));
                    ppainter->drawLine(static_cast<int>(cur_info.postion[6]), static_cast<int>(cur_info.postion[7]), 
                                       static_cast<int>(cur_info.postion[0]), static_cast<int>(cur_info.postion[1]));
                }
                //  未达到
                else
                {
                    //  当低于阈值也表示画图
                    if(now_low_flag)
                    {
                        ppainter->setPen(QPen(QColor(0,0,255), 3, Qt::DashLine));
                        ppainter->drawLine(static_cast<int>(cur_info.postion[0]), static_cast<int>(cur_info.postion[1]), 
                                           static_cast<int>(cur_info.postion[2]), static_cast<int>(cur_info.postion[3]));
                        ppainter->drawLine(static_cast<int>(cur_info.postion[2]), static_cast<int>(cur_info.postion[3]), 
                                           static_cast<int>(cur_info.postion[4]), static_cast<int>(cur_info.postion[5]));
                        ppainter->drawLine(static_cast<int>(cur_info.postion[4]), static_cast<int>(cur_info.postion[5]), 
                                           static_cast<int>(cur_info.postion[6]), static_cast<int>(cur_info.postion[7]));
                        ppainter->drawLine(static_cast<int>(cur_info.postion[6]), static_cast<int>(cur_info.postion[7]), 
                                           static_cast<int>(cur_info.postion[0]), static_cast<int>(cur_info.postion[1]));
                    }
                }

                //  已完成该数据的操作，擦除该数据
                ocr_info.erase(ocr_info.begin());
            }
            ppainter->end();

            //  构造输出结果
            SOCRout out_info;
            out_info.out_img = now_img;
            out_info.out_dat = ocr_buf_info;
            out_info.run_time_ns = run_time_ns;
            out_info.mode_type = now_type;

            //  发出通知信号
            emit send_ocr_result(out_info);

            //  完成一帧处理
            cmd_mutex.lock();
            this->cur_st = EOCRSt_Ready;
            now_cmd = EOCRcmd_Null;
            cmd_mutex.unlock();
        }
        //  非法命令
        else
        {
            //  释放CPU
            QThread::sleep(1);
            qDebug("Unknow cmd code!!");
        }
    }
}

CQtOCR::EOCRSt CQtOCR::GetStatus(void)
{
    EOCRSt re;
    cmd_mutex.lock();
    re = this->cur_st;
    cmd_mutex.unlock();
    return re;
}

void CQtOCR::ExOCR(QImage origin, CPyOCR::EModeType type)
{
    if(GetStatus() == EOCRSt_Busy) return;

    cmd_mutex.lock();
    this->cur_cmd = EOCRcmd_ExOCR;
    this->in_img_dat = origin;
    this->mode_type = type;
    this->cur_st = EOCRSt_Busy;    //  设置忙
    cmd_mutex.unlock();

    sem_cmd.release();
}

void CQtOCR::Release(void)
{
    cmd_mutex.lock();
    this->cur_cmd = EOCRcmd_Release;
    cmd_mutex.unlock();

    sem_cmd.release();
}
