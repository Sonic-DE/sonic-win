/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "toolsareaexceptionsmodel.h"

namespace
{
// Role → [ToolsArea] key of the exceptions' overrides.
QString keyOf(int role)
{
    switch (role) {
    case ToolsAreaExceptionsModel::ActiveOpacityRole:
        return QStringLiteral("ActiveOpacity");
    case ToolsAreaExceptionsModel::InactiveOpacityRole:
        return QStringLiteral("InactiveOpacity");
    case ToolsAreaExceptionsModel::ApplyOpacityToHeaderRole:
        return QStringLiteral("ApplyOpacityToHeader");
    case ToolsAreaExceptionsModel::BlurBehindRole:
        return QStringLiteral("BlurBehind");
    case ToolsAreaExceptionsModel::OpaqueWhenMaximizedRole:
        return QStringLiteral("OpaqueWhenMaximized");
    case ToolsAreaExceptionsModel::MatchApplicationColorRole:
        return QStringLiteral("MatchApplicationColor");
    default:
        return QString();
    }
}
}

ToolsAreaExceptionsModel::ToolsAreaExceptionsModel(KSharedConfigPtr config, QObject *parent)
    : QAbstractListModel(parent)
    , m_config(std::move(config))
{
    load();
}

int ToolsAreaExceptionsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_exceptions.size();
}

QHash<int, QByteArray> ToolsAreaExceptionsModel::roleNames() const
{
    return {{PatternRole, "pattern"},
            {EnabledRole, "exceptionEnabled"},
            {ActiveOpacityRole, "activeOpacity"},
            {InactiveOpacityRole, "inactiveOpacity"},
            {ApplyOpacityToHeaderRole, "applyOpacityToHeader"},
            {BlurBehindRole, "blurBehind"},
            {OpaqueWhenMaximizedRole, "opaqueWhenMaximized"},
            {MatchApplicationColorRole, "matchApplicationColor"},
            {IsDefaultRole, "isDefault"}};
}

QVariant ToolsAreaExceptionsModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }
    const KToolsArea::Exception &exception = m_exceptions.at(index.row());
    switch (role) {
    case PatternRole:
        return exception.pattern;
    case EnabledRole:
        return exception.enabled;
    case IsDefaultRole:
        return exception.isDefault;
    default:
        // Not overridden: null.
        return exception.overrides.value(keyOf(role));
    }
}

bool ToolsAreaExceptionsModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return false;
    }
    KToolsArea::Exception &exception = m_exceptions[index.row()];
    // A default exception can only be switched off or on.
    if (role == IsDefaultRole || (exception.isDefault && role != EnabledRole)) {
        return false;
    }
    if (role == PatternRole) {
        exception.pattern = value.toString();
    } else if (role == EnabledRole) {
        exception.enabled = value.toBool();
    } else if (const QString key = keyOf(role); !key.isEmpty()) {
        // A null value (from QML: null or undefined) removes the override.
        if (value.isNull() || !value.isValid()) {
            exception.overrides.remove(key);
        } else {
            exception.overrides.insert(key, value);
        }
    } else {
        return false;
    }
    Q_EMIT dataChanged(index, index, {role});
    setChanged(true);
    return true;
}

void ToolsAreaExceptionsModel::add()
{
    beginInsertRows({}, m_exceptions.size(), m_exceptions.size());
    m_exceptions.append(KToolsArea::Exception());
    endInsertRows();
    setChanged(true);
}

void ToolsAreaExceptionsModel::remove(int row)
{
    if (row < 0 || row >= m_exceptions.size() || m_exceptions.at(row).isDefault) {
        return;
    }
    beginRemoveRows({}, row, row);
    m_exceptions.removeAt(row);
    endRemoveRows();
    setChanged(true);
}

bool ToolsAreaExceptionsModel::isChanged() const
{
    return m_changed;
}

void ToolsAreaExceptionsModel::setChanged(bool changed)
{
    if (m_changed != changed) {
        m_changed = changed;
        Q_EMIT changedChanged();
    }
}

void ToolsAreaExceptionsModel::load()
{
    beginResetModel();
    m_config->reparseConfiguration();
    m_exceptions = KToolsArea::exceptions(m_config);
    endResetModel();
    setChanged(false);
}

void ToolsAreaExceptionsModel::save()
{
    KToolsArea::writeExceptions(m_config, m_exceptions);
    m_config->sync();
    setChanged(false);
}
