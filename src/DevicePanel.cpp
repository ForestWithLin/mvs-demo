#include "DevicePanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>

DevicePanel::DevicePanel(QWidget* parent)
    : QWidget(parent)
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);

    // 标题
    auto* titleLabel = new QLabel("设备列表");
    titleLabel->setStyleSheet("font-weight: bold; font-size: 13px; padding: 4px;");
    mainLayout->addWidget(titleLabel);

    // 树形列表
    m_tree = new QTreeWidget(this);
    m_tree->setHeaderLabels({"设备名称", "IP/序列号", "类型"});
    m_tree->setColumnWidth(0, 120);
    m_tree->setColumnWidth(1, 100);
    m_tree->setColumnWidth(2, 60);
    m_tree->setRootIsDecorated(false);
    m_tree->setAlternatingRowColors(true);
    m_tree->header()->setStretchLastSection(false);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    mainLayout->addWidget(m_tree);

    // 按钮栏
    auto* btnLayout = new QHBoxLayout();
    m_refreshBtn = new QPushButton("刷新");
    m_connectBtn = new QPushButton("连接");
    m_disconnectBtn = new QPushButton("断开");
    m_disconnectBtn->setEnabled(false);

    btnLayout->addWidget(m_refreshBtn);
    btnLayout->addWidget(m_connectBtn);
    btnLayout->addWidget(m_disconnectBtn);
    mainLayout->addLayout(btnLayout);

    // 连接信号
    connect(m_refreshBtn, &QPushButton::clicked, this, &DevicePanel::refreshRequested);
    connect(m_connectBtn, &QPushButton::clicked, this, &DevicePanel::onConnectClicked);
    connect(m_disconnectBtn, &QPushButton::clicked, this, &DevicePanel::onDisconnectClicked);
    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, &DevicePanel::onItemDoubleClicked);

    setMinimumWidth(250);
    setMaximumWidth(350);
}

void DevicePanel::updateDeviceList(const QList<DeviceInfo>& devices)
{
    m_devices = devices;
    m_tree->clear();

    for (const auto& dev : devices) {
        auto* item = new QTreeWidgetItem();
        item->setText(0, dev.name);
        item->setText(1, dev.transportType == "GigE" ? dev.ipAddress : dev.serialNumber);
        item->setText(2, dev.transportType);
        item->setData(0, Qt::UserRole, dev.indexInSdk);

        if (dev.isConnected) {
            item->setForeground(0, QBrush(QColor(0, 180, 0)));
        }

        m_tree->addTopLevelItem(item);
    }
}

void DevicePanel::onItemDoubleClicked(QTreeWidgetItem* item, int /*column*/)
{
    if (!item) return;
    int index = item->data(0, Qt::UserRole).toInt();
    emit connectRequested(index);
}

void DevicePanel::onConnectClicked()
{
    auto* item = m_tree->currentItem();
    if (!item) return;
    int index = item->data(0, Qt::UserRole).toInt();
    emit connectRequested(index);
}

void DevicePanel::onDisconnectClicked()
{
    emit disconnectRequested();
}

void DevicePanel::setConnectedState(bool connected)
{
    m_connectBtn->setEnabled(!connected);
    m_disconnectBtn->setEnabled(connected);
}
