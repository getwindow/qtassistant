﻿#include "AssistantModule.h"
#include "AssistantModule_p.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QPixmap>
#include <QStringBuilder>
#include <QTextStream>
#include <QUuid>
#include <QtDebug>
#include <QMetaEnum>

#include "SqlDatas/MasterLevels.h"
#include "SqlDatas/MemberBlacklist.h"
#include "SqlDatas/MemberWatchlist.h"

#include "HtmlDraw/HtmlDraw.h"
#include "AssistantFilters.h"

// class AssistantModule

AssistantModule::AssistantModule(CoolQ::ServiceEngine *engine)
    : CoolQ::ServiceModule(*new AssistantModulePrivate(), engine)
{
    Q_ASSERT(AssistantModulePrivate::instance == nullptr);
    AssistantModulePrivate::instance = this;

    Q_D(AssistantModule);

    d->levels = new MasterLevels(this);
    d->blacklist = new MemberBlacklist(this);
    d->watchlist = new MemberWatchlist(this);

    new HtmlDraw(usrFilePath("Materials"), this);

    d->checkTimerId = startTimer(10000);

    QFile file(usrFilePath("Assistant.json"));
    if (file.open(QFile::ReadOnly)) {
        auto doc = QJsonDocument::fromJson(file.readAll());
        if (!doc.isEmpty() && doc.isObject()) {
            d->init(doc.object());
        }
    }

    d->levels->init(d->superUsers);

    // Private Commands

    new PrivateCleanDataCaches(this);
    new PrivateRestartComputer(this);

    new PrivateCreateStartupShortcut(this);
    new PrivateDeleteStartupShortcut(this);

    // Group Commands

    new GroupBanHongbaoAction(d->banHongbaoGroups, this);

    new GroupCommandsAction(this);

    new GroupRenameMemberAction(this);
    new GroupMemberLevelAction(this);
    new GroupFormatMemberAction(this);

    new GroupBanMemberAction(this);
    new GroupKickMemberAction(this);
    new GroupRaiseMemberAction(this);

    new GroupUnbanMemberAction(this);
    new GroupLowerMemberAction(this);

    new GroupWatchlistAction(this);
    new GroupBlacklistAction(this);

    new GroupMemberInfoAction(this);

    // Group Help Commands

    new GroupRenameMemberHelpAction(this);
    new GroupMemberLevelHelpAction(this);
    new GroupFormatMemberHelpAction(this);

    new GroupBanMemberHelpAction(this);
    new GroupKickMemberHelpAction(this);
    new GroupRaiseMemberHelpAction(this);

    new GroupUnbanMemberHelpAction(this);
    new GroupLowerMemberHelpAction(this);

    new GroupWatchlistHelpAction(this);
    new GroupBlacklistHelpAction(this);

    new GroupMemberInfoHelpAction(this);
}

AssistantModule::~AssistantModule()
{
    Q_D(AssistantModule);

    AssistantModulePrivate::instance = nullptr;

    killTimer(d->checkTimerId);
}

AssistantModule *AssistantModule::instance()
{
    return AssistantModulePrivate::instance;
}

MasterLevel AssistantModule::level(qint64 gid, qint64 uid) const
{
    Q_D(const AssistantModule);

    return d->levels->level(gid, uid);
}

void AssistantModule::timerEvent(QTimerEvent *)
{
    Q_D(AssistantModule);

    // 检查新手名单。
    do {
        CoolQ::MemberList members;
        d->watchlist->expiredMembers(members);
        for (const auto &member : members) {
            CoolQ::MemberInfo mi = memberInfo(member.first, member.second, false);
            if (mi.isValid() && mi.lastSent().isNull()) {
                kickGroupMember(member.first, member.second, false);
            }
        }
    } while (false);
}

void AssistantModule::permissionDenied(qint64 gid, qint64 uid, MasterLevel level, const QString &reason)
{
    Q_UNUSED(uid);

    QString content = reason.isEmpty() ? QString(u8"作为一个%1，你没有权限执行此操作。").arg(MasterLevels::levelName(level)) : reason;
    QString html = QString("<html><body><span class=\"t\">%1</span><p class=\"c\">%2</p></body></html>").arg(QString(u8"没有相关权限"), content);

    QPixmap feedback = HtmlDraw::drawDangerText(html, 400, gid);
    QString fileName = saveImage(feedback);
    sendGroupMessage(gid, image(fileName));
}

void AssistantModule::showPrimary(qint64 gid, const QString &title, const QString &content)
{
    feedback(gid, title, content, HtmlDraw::Primary);
}

void AssistantModule::showDanger(qint64 gid, const QString &title, const QString &content)
{
    feedback(gid, title, content, HtmlDraw::Danger);
}

void AssistantModule::showWarning(qint64 gid, const QString &title, const QString &content)
{
    feedback(gid, title, content, HtmlDraw::Warning);
}

void AssistantModule::showPrompt(qint64 gid, const QString &title, const QString &content)
{
    feedback(gid, title, content, HtmlDraw::Prompt);
}

void AssistantModule::showSuccess(qint64 gid, const QString &title, const QString &content)
{
    feedback(gid, title, content, HtmlDraw::Success);
}

void AssistantModule::feedback(qint64 gid, const QString &title, const QString &content, HtmlDraw::Style style)
{
    QString html = QString("<html><body><span class=\"t\">%1</span><p class=\"c\">%2</p></body></html>").arg(title, content);

    QPixmap feedback = HtmlDraw::drawText(html, style, 400, gid);
    QString fileName = saveImage(feedback);
    sendGroupMessage(gid, image(fileName));
}

void AssistantModule::showPrimaryList(qint64 gid, const QString &title, const LevelInfoList &members, bool level)
{
    feedbackList(gid, title, members, level, HtmlDraw::Primary);
}

void AssistantModule::showDangerList(qint64 gid, const QString &title, const LevelInfoList &members, bool level)
{
    feedbackList(gid, title, members, level, HtmlDraw::Danger);
}

void AssistantModule::showWarningList(qint64 gid, const QString &title, const LevelInfoList &members, bool level)
{
    feedbackList(gid, title, members, level, HtmlDraw::Warning);
}

void AssistantModule::showPromptList(qint64 gid, const QString &title, const LevelInfoList &members, bool level)
{
    feedbackList(gid, title, members, level, HtmlDraw::Prompt);
}

void AssistantModule::showSuccessList(qint64 gid, const QString &title, const LevelInfoList &members, bool level)
{
    feedbackList(gid, title, members, level, HtmlDraw::Success);
}

void AssistantModule::feedbackList(qint64 gid, const QString &title, const LevelInfoList &members, bool level, HtmlDraw::Style style)
{
    for (int i = 0, part = 0; i < members.count();) {
        QString html;
        do {
            int cc = i + 4;
            QTextStream ds(&html);
            ds << "<html><body><span class=\"t\">" << title;
            if (members.count() > 5) {
                ds << QString(u8"(第 %1 部分)").arg(++part);
            }
            ds << "</span><div>";
            for (; i < members.count(); ++i) {
                const LevelInfo &li = members.at(i);
                ds << "<p class=\"c\">";
                if (level) {
                    ds << MasterLevels::levelName(li.level) << QString(u8": ");
                }
                CoolQ::MemberInfo mi = memberInfo(gid, li.uid);
                if (mi.isValid()) {
                    if (!mi.nameCard().isEmpty()) {
                        ds << mi.nameCard();
                    } else {
                        ds << mi.nickName();
                    }
                } else {
                    CoolQ::PersonInfo pi = personInfo(li.uid);
                    if (pi.isValid()) {
                        ds << pi.nickName();
                    } else {
                        ds << QString::number(li.uid);
                    }
                    ds << "<span>(" << QString(u8"不在本群") << ")</span>";
                }
                ds << "</p>";

                if (i == cc) {
                    ++i;
                    break;
                }
            }
            ds << "</div></body></html>";
        } while (false);

        QPixmap feedback = HtmlDraw::drawText(html, style, 400, gid);
        QString fileName = saveImage(feedback);
        sendGroupMessage(gid, image(fileName));
    }
}

void AssistantModule::showWelcomes(qint64 gid, qint64 uid)
{
    Q_D(AssistantModule);

    MasterLevel level = d->levels->level(gid, uid);
    if (MasterLevel::ATField < level) {
        return;
    }

    QString msg;
    QTextStream ts(&msg);

    QFileInfo rootInfo(usrFilePath(QString("Welcomes/%1").arg(gid)));
    if (rootInfo.isDir()) {
        int i = 0;
        QDir root(rootInfo.absoluteFilePath());
        for (auto fileInfo : root.entryInfoList(QDir::Files, QDir::Name)) {
            if (fileInfo.suffix() == "txt") {
                QStringList nameParts = fileInfo.fileName().split('.');
                if (nameParts.count() == 3) {
                    QFile file(fileInfo.absoluteFilePath());
                    if (file.open(QFile::ReadOnly)) {
                        auto html = QString::fromUtf8(file.readAll());
                        auto styleEnum = QMetaEnum::fromType<HtmlDraw::Style>();
                        auto style = styleEnum.keysToValue(nameParts.at(1).toLatin1());
                        auto image = d->htmlDraw->drawText(html, (HtmlDraw::Style)style, 400, gid);
                        if (++i > 1) ts << '\n';
                        ts << this->image(saveImage(image));
                    }
                }
            }
        }
    }

    ts.flush();

    sendGroupMessage(gid, msg);
}

void AssistantModule::saveWelcomes(qint64 gid, qint64 uid)
{
    Q_D(AssistantModule);

    MasterLevel level = d->levels->level(gid, uid);
    if (MasterLevel::ATField < level) {
        return;
    }

    QFileInfo rootInfo(imgFilePath("Welcomes"));
    if (rootInfo.isDir()) {
        QDir root(rootInfo.absoluteFilePath());
        for (auto fileInfo : root.entryInfoList(QDir::Files, QDir::Name)) {
            if (fileInfo.suffix() == "txt") {
                QStringList nameParts = fileInfo.fileName().split('.');
                if (nameParts.count() == 3) {
                    QFile file(fileInfo.absoluteFilePath());
                    if (file.open(QFile::ReadOnly)) {
                        auto html = QString::fromUtf8(file.readAll());
                        auto styleEnum = QMetaEnum::fromType<HtmlDraw::Style>();
                        auto style = styleEnum.keysToValue(nameParts.at(1).toLatin1());
                        auto image = HtmlDraw::drawText(html, (HtmlDraw::Style)style, 400, gid);
                        auto fileName = imgFilePath(QString("Welcomes/%1.PNG").arg(nameParts.at(0)));
                        if (image.save(fileName, "PNG")) {
                            qInfo() << "Save Welcomes:" << fileName;
                        } else {
                            qInfo() << "Save Welcomes failed:" << fileName;
                        }
                    }
                }
            }
        }
    }

    sendGroupMessage(gid, QString(u8"Save Welcomes finished"));
}

// class AssistantModulePrivate

AssistantModule *AssistantModulePrivate::instance = nullptr;

AssistantModulePrivate::AssistantModulePrivate()
    : levels(Q_NULLPTR)
    , watchlist(Q_NULLPTR)
    , blacklist(Q_NULLPTR)
    , htmlDraw(Q_NULLPTR)
    , checkTimerId(-1)
{
}

AssistantModulePrivate::~AssistantModulePrivate()
{
}

LevelInfoList AssistantModulePrivate::findUsers(const QStringList &args)
{
    QListIterator<QString> i(args);
    LevelInfoList levels;
    i.toBack();

    while (i.hasPrevious()) {
        const QString &arg = i.previous();
        if (arg.startsWith(QStringLiteral("[CQ:at"))) {
            qint64 uid = arg.mid(10, arg.count() - 11).toLongLong();
            if (100000 > uid) {
                break;
            }

            bool noFound = true;
            for (const LevelInfo &li : levels) {
                if (li.uid == uid) {
                    noFound = false;
                    break;
                }
            }
            if (noFound) {
                levels.prepend(LevelInfo(uid, MasterLevel::Unknown));
            }
        } else {
            qint64 uid = arg.toLongLong();
            if (100000 > uid) {
                break;
            }

            bool noFound = true;
            for (const LevelInfo &li : levels) {
                if (li.uid == uid) {
                    noFound = false;
                    break;
                }
            }
            if (noFound) {
                levels.prepend(LevelInfo(uid, MasterLevel::Unknown));
            }
        }
    }

    return levels;
}

void AssistantModulePrivate::safetyNameCard(QString &nameCard)
{
    // 屏蔽 emoji 表情
    do {
        int s = nameCard.indexOf("[CQ:emoji");
        if (s < 0) {
            break;
        }
        int e = nameCard.indexOf(']', s);
        if (e < 0) {
            break;
        }

        nameCard.replace(s, e - s + 1, "{?}");
    } while (true);
}

void AssistantModulePrivate::formatNameCard(QString &nameCard)
{
    // 在这里，我们对新名片做规范化处理。
    nameCard.remove(' '); // 消除空格，不允许有空格。
    nameCard.replace("【", "["); // 替换全角方括号，用半角方括号替代。
    nameCard.replace("】", "]"); // 替换全角方括号，用半角方括号替代。
}

void AssistantModulePrivate::init(const QJsonObject &o)
{
    QJsonArray superUsers = o.value("superUsers").toArray();
    for (int i = 0; i < superUsers.count(); ++i)
        this->superUsers.insert(superUsers.at(i).toString().toLongLong());

    QJsonArray managedGroups = o.value("managedGroups").toArray();
    for (int i = 0; i < managedGroups.count(); ++i)
        this->managedGroups.insert(managedGroups.at(i).toString().toLongLong());

    QJsonArray banHongbaoGroups = o.value("banHongbaoGroups").toArray();
    for (int i = 0; i < banHongbaoGroups.count(); ++i)
        this->banHongbaoGroups.insert(banHongbaoGroups.at(i).toString().toLongLong());

    qInfo() << QString(u8"超级用户") << this->superUsers;
    qInfo() << QString(u8"管理群组") << this->managedGroups;
    qInfo() << QString(u8"屏蔽红包") << this->banHongbaoGroups;
}

void AssistantModulePrivate::saveWelcomes(const QString &id, HtmlDraw::Style style)
{
    Q_Q(AssistantModule);

    QFile file(QString(":/Welcomes/%1.utf8.txt").arg(id));
    if (file.open(QFile::ReadOnly)) {
        auto htmlText = QString::fromUtf8(file.readAll());

        QPixmap image = htmlDraw->drawText(htmlText, style, 400, 0);
        if (image.save(q->imgFilePath(QString("Welcomes/%1.png").arg(id)), "PNG")) {
            qInfo() << "Output 1";
        } else {
            qInfo() << "Output 2";
        }
    }

    qInfo() << "failed";
}
