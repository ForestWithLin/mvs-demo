#ifndef MVSVIEWER_CAMERA_PARAM_PANEL_H
#define MVSVIEWER_CAMERA_PARAM_PANEL_H

#include <QWidget>
#include <QTreeWidget>
#include <QMap>
#include <QVariant>
#include "Types.h"

class CameraEngine;

class CameraParamPanel : public QWidget
{
    Q_OBJECT
public:
    explicit CameraParamPanel(QWidget* parent = nullptr);

public slots:
    void onParamReadFinished(const QString& key, const QVariant& value);
    void clearParams();
    void requestAllParams();

signals:
    void paramChangeRequested(const QString& key, const QVariant& value);
    void paramReadRequested(const QString& key);

private slots:
    void onItemChanged(QTreeWidgetItem* item, int column);

private:
    void buildParamList();

    QTreeWidget* m_tree;

    // 预定义的参数列表
    QList<CameraParamInfo> m_paramList;
    QMap<QString, QTreeWidgetItem*> m_paramItems;
};

#endif // MVSVIEWER_CAMERA_PARAM_PANEL_H
