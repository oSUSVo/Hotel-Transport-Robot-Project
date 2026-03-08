#include <QDateTime>
#include <QTimer>
#include <QLabel>
#include <QDebug>
#include <cmath>
#include "mainwidget.h"
#include "ui_mainwidget.h"
#include "rosnode_lidar_cam.h" //캠 추가
#include <QDesktopServices> //버튼 추가
#include <QUrl>
#include <cstdlib> // system() 함수를 사용


MainWidget::MainWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::MainWidget)
{
    ui->setupUi(this);

    ui->stateimg_lb1->setPixmap(QPixmap(":/src/bot3_goal_qt/QTimage/transport_12859042.png"));
    ui->stateimg_lb2->setPixmap(QPixmap(":/src/bot3_goal_qt/QTimage/dot.png"));
    ui->stateimg_lb3->setPixmap(QPixmap(":/src/bot3_goal_qt/QTimage/robot_arm1.png"));
    ui->stateimg_lb4->setPixmap(QPixmap(":/src/bot3_goal_qt/QTimage/dot.png"));
    ui->stateimg_lb5->setPixmap(QPixmap(":/src/bot3_goal_qt/QTimage/elevator.png"));
    ui->stateimg_lb6->setPixmap(QPixmap(":/src/bot3_goal_qt/QTimage/dot.png"));
    ui->stateimg_lb7->setPixmap(QPixmap(":/src/bot3_goal_qt/QTimage/room.png"));

    //카메라, 라이다 연동
    RosNodeLidarCam *rosNode = new RosNodeLidarCam(this);
    rosNode->pLcamView = ui->label_camView;

    pRosNodeAction = new RosNodeAction(this);
    this->setWindowTitle("Hotel");
//    pMultiThreadedExecutor = new rclcpp::executors::MultiThreadedExecutor;
    pRosNode = new RosNode(this);

    connect(pRosNode, SIGNAL(batteryLcdDisplaySig(double, double)), this, SLOT(batteryLcdDisplaySlot(double, double)));
 //   Robot mission info
    connect(pRosNode, SIGNAL(robotStateSig(QString)), this, SLOT(updateRobotStateSig(QString)));
 //   pRosNodeAction->send_goal(1.0,2.0);
    // connect(ui->start_pb, &QPushButton::clicked, this, [=](){

    //     QString roomNumber = ui->rn_le->text();   // 201 입력
    //     pRosNodeAction->requestDelivery(roomNumber);

    // });
    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [=](){
        ui->date2_lb->setText(QDateTime::currentDateTime().toString("yyyy-MM-dd"));
		ui->time2_lb->setText(QDateTime::currentDateTime().toString("AP hh:mm:ss"));
    });

    timer->start(1000);
}

MainWidget::~MainWidget()
{
    delete ui;
}

void MainWidget::batteryLcdDisplaySlot(double voltage, double percentage)
{
    ui->pLNVolt->display((int)(round(voltage)));
    ui->pLNPercent->display((int)(round(percentage)));
}

void MainWidget::on_pPBHome_clicked()
{

//    pRosNode->sendMissionMessage("WAIT_LOCATION");
    if(!pRosNodeAction) return;
    pRosNodeAction->requestDelivery("WAIT_LOCATION");
    qDebug() << "[명령 전송] 복귀: WAIT_LOCATION";
}

void MainWidget::on_pPBLuggage_clicked()
{

//    pRosNode->sendMissionMessage("ARM");
     if(!pRosNodeAction) return;
     pRosNodeAction->requestDelivery("ARM");
     qDebug() << "[명령 전송] 이동: ARM (Luggage)";
}

void MainWidget::on_pPBRoom_clicked()
{/*
    QString room = ui->CBRoomnum->currentText();
    QString roomNum = room.left(3);

    pRosNode->sendMissionMessage("ROOM_" + roomNum);*/
    if(!pRosNodeAction) return;
    QString room = ui->CBRoomnum->currentText(); // 예: "201호" 또는 "201"
    QString roomNum = room.left(3);              // 앞 3글자 추출 (예: "201")

     if(roomNum.isEmpty()) return;

   // 미션 노드는 "ROOM_201" 형태를 기대하므로 requestDelivery 내부 로직을 따름
   // pRosNodeAction->requestDelivery(roomNum);
    QString fullRoomId = "ROOM_" + roomNum;
    pRosNodeAction->requestDelivery(fullRoomId);
   // Flask DB 업데이트 (on_start_pb_clicked와 동일한 로직 추가)
    QString cmd = QString("curl -s \"http://10.10.141.40:5000/api/checkin?room=%1\" &").arg(roomNum);
    system(cmd.toStdString().c_str());

    qDebug() << "[명령 전송] 배송 시작: ROOM_" << fullRoomId; //roomNum;

}

void MainWidget::on_start_pb_clicked()
{
    // 1. 입력창에서 호수 가져오기
    QString roomText = ui->rn_le->text();
    if(roomText.isEmpty() || roomText == "0") return;

    // 2. [ROS 2] 미션 노드에 로봇 출발 서비스 요청 (기존 코드)
    pRosNodeAction->sendRoomNumber(roomText.toInt());

    // 3. [HTTP] 웹 서버(Flask)로 DB 저장 요청 날리기 (curl 명령어 활용)
    // -s: 진행창 숨김 / &: 백그라운드 실행 (프로그램 멈춤 방지)
    QString cmd = QString("curl -s \"http://10.10.141.40:5000/api/checkin?room=%1\" &").arg(roomText);

    // 명령어 실행 (예: curl -s "http://10.10.141.40:5000/api/checkin?room=201" &)
    system(cmd.toStdString().c_str());

    // 4. 입력창 초기화
    ui->rn_le->clear();
    qDebug() << "[DB 체크인 전송 완료] 호수:" << roomText;
}

void MainWidget::updateRobotStateSig(QString state)
{
    qDebug() << "현재 수신된 상태 값:" << state;

    // 1) "STEP=XXXX" 형태면 step만 추출 (없으면 원문 사용)
    QString step = state;
    int idx = state.indexOf("STEP=");
    if (idx >= 0) {
        int start = idx + 5;
        int end = state.indexOf(' ', start);
        step = (end > start) ? state.mid(start, end - start) : state.mid(start);
    }

    QPixmap pix;
    QLabel* targetLabel = nullptr;

    auto set = [&](const char* path, QLabel* lb) {
        if (!pix.load(path)) {
            qDebug() << "❌ 이미지 로드 실패:" << path << " step=" << step;
            return false;
        }
        targetLabel = lb;
        return true;
    };

    bool ok = false;

    // 2) hotel_mission 최종 step_name 기준으로 정확히 매핑
    if (step == "BOOT") {
        ok = set(":/src/bot3_goal_qt/QTimage/bot.png", ui->stateimg_lb8);
    }
    else if (step == "IDLE") {
        ok = set(":/src/bot3_goal_qt/QTimage/bot.png", ui->stateimg_lb8);
    }
    else if (step == "NAV_TO_ARM") {
        ok = set(":/src/bot3_goal_qt/QTimage/rightmovingbot.png", ui->stateimg_lb9);
    }
    else if (step == "ARM_LOAD") {
        ok = set(":/src/bot3_goal_qt/QTimage/bot.png", ui->stateimg_lb10);
    }

    // 엘리베이터 “앞/복귀/하차” 이동 구간
    else if (step == "NAV_TO_ELEV_1F_FRONT" ||
             step == "NAV_TO_ELEV_2F_FRONT" ||
             step == "NAV_TO_ELEV_1F_FRONT_BACK" ||
             step == "NAV_BACK_TO_ELEV_2F_FRONT")
    {
        ok = set(":/src/bot3_goal_qt/QTimage/rightmovingbot.png", ui->stateimg_lb11);
        // 복귀 방향이면 left 이미지로 바꾸고 싶으면 여기서 조건 분기 가능
        if (step == "NAV_BACK_TO_ELEV_2F_FRONT" || step == "NAV_TO_ELEV_1F_FRONT_BACK") {
            ok = set(":/src/bot3_goal_qt/QTimage/leftmovingbot.png", ui->stateimg_lb11);
        }
    }

    // 엘리베이터 “호출/탑승/이동” 작업 구간
    else if (step == "ELEV_CALL_1F" || step == "ELEV_GO_2F" ||
             step == "NAV_TO_ELEV_INSIDE_1F" ||
             step == "ELEV_CALL_2F" || step == "ELEV_GO_1F" ||
             step == "NAV_TO_ELEV_INSIDE_2F")
    {
        ok = set(":/src/bot3_goal_qt/QTimage/bot.png", ui->stateimg_lb12);
    }

    else if (step == "NAV_TO_ROOM") {
        ok = set(":/src/bot3_goal_qt/QTimage/rightmovingbot.png", ui->stateimg_lb13);
    }
    else if (step == "ROOM_DELIVER_WAIT") {
        ok = set(":/src/bot3_goal_qt/QTimage/bot.png", ui->stateimg_lb14);
    }

    else if (step == "NAV_TO_WAIT") {
        ok = set(":/src/bot3_goal_qt/QTimage/leftmovingbot.png", ui->stateimg_lb9);
    }

    // DIRECT_* (request_goto)도 UI가 흔들리지 않게 처리
    else if (step.startsWith("DIRECT_")) {
        // 단순하게 "이동중"으로 표시
        ok = set(":/src/bot3_goal_qt/QTimage/rightmovingbot.png", ui->stateimg_lb11);
    }

    // 3) 매칭 성공했을 때만 clear 후 표시 (중간 끊김 방지 핵심)
    if (ok && targetLabel) {
        ui->stateimg_lb8->clear();
        ui->stateimg_lb9->clear();
        ui->stateimg_lb10->clear();
        ui->stateimg_lb11->clear();
        ui->stateimg_lb12->clear();
        ui->stateimg_lb13->clear();
        ui->stateimg_lb14->clear();

        targetLabel->setPixmap(
            pix.scaled(targetLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation)
        );
        qDebug() << "✅ step=" << step << " 에 대한 UI 표시 성공";
    } else {
        // 매칭 실패 시에는 "지우지 않고 유지" → 멈춘 것처럼 보이는 현상 제거
        qDebug() << "⚠️ UI 매칭 없음. step=" << step << " (표시는 유지)";
    }
}

void MainWidget::on_pushButton_clicked()
 {
    // 1. 연결할 주소를 입력합니다.
    QString url = "http://10.10.141.40:5000/";

    // 2. 시스템의 기본 브라우저를 사용하여 해당 주소를 엽니다.
    QDesktopServices::openUrl(QUrl(url));

 }

void MainWidget::on_checkout_pb_clicked()
{
    // 1. 입력창(rn_le)에서 호수 가져오기
    QString roomText = ui->rn_le->text();

    // 빈 값이나 0이면 중단
    if(roomText.isEmpty() || roomText == "0") return;

    // 2. [HTTP] 웹 서버(Flask)로 체크아웃(checkout) 요청 날리기
    // 로봇 서비스(pRosNodeAction) 호출은 제외하여 로봇은 움직이지 않습니다.
    QString cmd = QString("curl -s \"http://10.10.141.40:5000/api/checkout?room=%1\" &").arg(roomText);

    // 명령어 실행
    system(cmd.toStdString().c_str());

    // 3. 입력창 초기화 및 로그 출력
    ui->rn_le->clear();
    qDebug() << "[DB 체크아웃 전송 완료] 호수:" << roomText;
}

void MainWidget::on_stop_pb_clicked()
{
    if(!pRosNodeAction) return;

    // 1. [ROS 2] 미션 노드에 "EMERGENCY_STOP" 명령 전송
    // 미션 노드에서 이 명령을 받으면 busy_ = false, step = IDLE로 전환하도록 설계합니다.
    pRosNodeAction->requestDelivery("EMERGENCY_STOP");

    // 2. [HTTP] Flask 서버 DB 업데이트 (상태를 STOP으로 기록)
    QString cmd = "curl -s \"http://10.10.141.40:5000/api/checkin?room=STOP\" &";
    system(cmd.toStdString().c_str());

    qDebug() << "🚨 [긴급 정지] 모든 미션 취소 및 정지";
}

void MainWidget::on_turn_pb_clicked()
{
    if(!pRosNodeAction) return;

    // 1. [ROS 2] 미션 노드에 "EMERGENCY_RETURN" 명령 전송
    // 미션 노드는 이를 받으면 즉시 NAV_TO_WAIT 단계로 강제 전이합니다.
    pRosNodeAction->requestDelivery("WAIT_LOCATION");

    // 2. [HTTP] Flask 서버 DB 업데이트
    QString cmd = "curl -s \"http://10.10.141.40:5000/api/checkin?room=RETURN\" &";
    system(cmd.toStdString().c_str());

    qDebug() << "🔄 [긴급 복귀] 대기 장소로 즉시 회항";
}
