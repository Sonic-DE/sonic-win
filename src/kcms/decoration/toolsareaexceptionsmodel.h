/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <KSharedConfig>
#include <KToolsArea>

#include <QAbstractListModel>

/**
 * The per-application exceptions to the titlebar transparency settings
 * (KToolsArea::exceptions(), the built-in default ones first): a pattern, on/off, and
 * each transparency setting overridden (its value) or not (null). A default exception
 * can only be switched off.
 */
class ToolsAreaExceptionsModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(bool changed READ isChanged NOTIFY changedChanged)

public:
    enum Role {
        PatternRole = Qt::UserRole + 1,
        EnabledRole,
        ActiveOpacityRole,
        InactiveOpacityRole,
        ApplyOpacityToHeaderRole,
        BlurBehindRole,
        OpaqueWhenMaximizedRole,
        MatchApplicationColorRole,
        // A built-in default exception: it can only be switched off.
        IsDefaultRole,
    };
    Q_ENUM(Role)

    explicit ToolsAreaExceptionsModel(KSharedConfigPtr config, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void add();
    Q_INVOKABLE void remove(int row);

    bool isChanged() const;
    void load();
    void save();

Q_SIGNALS:
    void changedChanged();

private:
    void setChanged(bool changed);

    KSharedConfigPtr m_config;
    QList<KToolsArea::Exception> m_exceptions;
    bool m_changed = false;
};
