#include "CameraParamPanel.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMetaObject>
#include <QMetaType>
#include <QSet>

CameraParamPanel::CameraParamPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    auto* titleLabel = new QLabel("相机参数");
    titleLabel->setStyleSheet("font-weight: bold; font-size: 13px; padding: 4px;");
    layout->addWidget(titleLabel);

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderLabels({"参数", "值"});
    m_tree->setColumnWidth(0, 130);
    m_tree->setRootIsDecorated(true);
    m_tree->setAlternatingRowColors(true);
    m_tree->header()->setStretchLastSection(true);
    m_tree->setEditTriggers(QTreeWidget::DoubleClicked);

    layout->addWidget(m_tree);

    connect(m_tree, &QTreeWidget::itemChanged, this, &CameraParamPanel::onItemChanged);

    setMinimumWidth(220);
    setMaximumWidth(350);

    buildParamList();
}

void CameraParamPanel::buildParamList()
{
    m_paramList = {
        // 曝光
        {"ExposureTime",       "曝光时间(us)",   "曝光", "float"},
        {"Gain",               "增益(dB)",       "曝光", "float"},
        {"AutoExposureTime",   "自动曝光",       "曝光", "enum"},

        // 图像
        {"Width",              "宽度",           "图像", "int"},
        {"Height",             "高度",           "图像", "int"},
        {"AcquisitionFrameRate","帧率(fps)",     "图像", "float"},
        {"PixelFormat",        "像素格式",       "图像", "enum"},

        // 颜色
        {"BalanceWhiteAuto",   "自动白平衡",     "颜色", "enum"},
        {"BalanceRatio",       "白平衡比例",     "颜色", "int"},

        // 触发
        {"TriggerMode",        "触发模式",       "触发", "enum"},
        {"TriggerSource",      "触发源",         "触发", "enum"},
    };
}

void CameraParamPanel::requestAllParams()
{
    m_tree->clear();
    m_paramItems.clear();

    // 按分组创建顶层节点
    QMap<QString, QTreeWidgetItem*> groups;
    QSet<QString> addedGroups;
    for (const auto& param : m_paramList) {
        if (!addedGroups.contains(param.category)) {
            auto* groupItem = new QTreeWidgetItem(m_tree);
            groupItem->setText(0, param.category);
            groupItem->setFlags(groupItem->flags() & ~Qt::ItemIsEditable);
            groups[param.category] = groupItem;
            addedGroups.insert(param.category);
        }
    }

    // 添加参数到对应分组
    for (const auto& param : m_paramList) {
        auto* item = new QTreeWidgetItem(groups[param.category]);
        item->setText(0, param.displayName);
        item->setText(1, "--");
        item->setData(0, Qt::UserRole, param.key);
        item->setData(0, Qt::UserRole + 1, param.type);
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        m_paramItems[param.key] = item;

        // 跨线程异步请求参数值
        emit paramReadRequested(param.key);
    }
}

void CameraParamPanel::onParamReadFinished(const QString& key, const QVariant& value)
{
    auto it = m_paramItems.find(key);
    if (it == m_paramItems.end()) return;

    QString display;
    switch (value.metaType().id()) {
    case QMetaType::Double:
        display = QString::number(value.toDouble(), 'f', 2);
        break;
    case QMetaType::Int:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
        display = QString::number(value.toLongLong());
        break;
    default:
        display = value.toString();
        break;
    }

    it.value()->setText(1, display);
}

void CameraParamPanel::clearParams()
{
    m_tree->clear();
    m_paramItems.clear();
}

void CameraParamPanel::onItemChanged(QTreeWidgetItem* item, int column)
{
    if (column != 1 || !item) return;

    QString key = item->data(0, Qt::UserRole).toString();
    QString type = item->data(0, Qt::UserRole + 1).toString();
    QString valueStr = item->text(1);

    QVariant value;
    if (type == "float") {
        value = valueStr.toDouble();
    } else {
        value = valueStr.toInt();
    }

    emit paramChangeRequested(key, value);
}
