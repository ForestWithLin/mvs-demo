#ifndef MVSVIEWER_DEVICE_PANEL_H
#define MVSVIEWER_DEVICE_PANEL_H

#include <QWidget>
#include <QTreeWidget>
#include <QPushButton>
#include <QComboBox>
#include "Types.h"

class DevicePanel : public QWidget
{
    Q_OBJECT
public:
    explicit DevicePanel(QWidget* parent = nullptr);

    void updateDeviceList(const QList<DeviceInfo>& devices);
    void setConnectedState(bool connected);
    void setSdk(CameraSdk sdk);

signals:
    void sdkChangeRequested(CameraSdk sdk);
    void connectRequested(int index);
    void disconnectRequested();
    void refreshRequested();

public slots:
    void onConnectClicked();
    void onDisconnectClicked();

private slots:
    void onItemDoubleClicked(QTreeWidgetItem* item, int column);

private:
    QComboBox* m_sdkCombo;
    QTreeWidget* m_tree;
    QPushButton* m_connectBtn;
    QPushButton* m_disconnectBtn;
    QPushButton* m_refreshBtn;
    QList<DeviceInfo> m_devices;
};

#endif // MVSVIEWER_DEVICE_PANEL_H
