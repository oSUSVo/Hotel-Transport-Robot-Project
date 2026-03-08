#ifndef MAINWIDGET_H
#define MAINWIDGET_H

#include <QWidget>
#include <QPushButton>
#include <QTimer>
#include "rosnode.h"
#include "rosnode_action.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWidget;
}
QT_END_NAMESPACE

class MainWidget : public QWidget
{
    Q_OBJECT

public:
    MainWidget(QWidget *parent = nullptr);
    ~MainWidget();

private slots:
    void on_start_pb_clicked();
    void updateRobotStateSig(QString state);
    void batteryLcdDisplaySlot(double, double);
    void on_pPBHome_clicked();
    void on_pPBLuggage_clicked();
    void on_pPBRoom_clicked();
    // void on_pPushButtonGo_clicked();

    void on_pushButton_clicked();
    void on_checkout_pb_clicked();

    void on_turn_pb_clicked();
    void on_stop_pb_clicked();
private:
    Ui::MainWidget *ui;
    rclcpp::executors::MultiThreadedExecutor *pMultiThreadedExecutor;
    RosNode *pRosNode;
    RosNodeAction *pRosNodeAction;
    double linearX;
    double angularZ;
};
#endif // MAINWIDGET_H
