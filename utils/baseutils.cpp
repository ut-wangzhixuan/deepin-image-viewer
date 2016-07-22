#include "baseutils.h"
#include <stdio.h>
#include <fcntl.h>
#include <fstream>
#include <linux/fs.h>
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDesktopServices>
#include <QDBusInterface>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDir>
#include <QFontMetrics>
#include <QFileInfo>
#include <QImage>
#include <QMimeData>
#include <QUrl>
#include <QDebug>
#include <QTextStream>

namespace utils {

namespace base {

const QString DATETIME_FORMAT_NORMAL = "yyyy.MM.dd";
const QString DATETIME_FORMAT_EXIF = "yyyy:MM:dd HH:mm:ss";

QString sizeToHuman(const qlonglong bytes)
{
    qlonglong sb = 1024;
    if (bytes < sb) {
        return QString::number(bytes) + " B";
    }
    else if (bytes < sb * sb) {
        return QString::number((double)bytes / sb, 'f', 1) + " KB";
    }
    else if (bytes < sb * sb * sb) {
        return QString::number((double)bytes / sb / sb, 'f', 1) + " MB";
    }
    else {
        return QString::number(bytes);
    }
}

QString timeToString(const QDateTime &time, bool normalFormat)
{
    if (normalFormat)
        return time.toString(DATETIME_FORMAT_NORMAL);
    else
        return time.toString(DATETIME_FORMAT_EXIF);
}

QString formatExifTimeString(const QString &exifTimeStr)
{
    QDateTime dt = QDateTime::fromString(exifTimeStr, DATETIME_FORMAT_EXIF);
    return dt.toString(DATETIME_FORMAT_NORMAL);
}

int stringWidth(const QFont &f, const QString &str)
{
    QFontMetrics fm(f);
    return fm.width(str);
}

QDateTime stringToDateTime(const QString &time)
{
    QDateTime dt = QDateTime::fromString(time, DATETIME_FORMAT_EXIF);
    if (! dt.isValid()) {
        dt = QDateTime::fromString(time, DATETIME_FORMAT_NORMAL);
    }
    return dt;
}

void showInFileManager(const QString &path)
{
    if (path.isEmpty() || !QFile::exists(path)) {
        return;
    }
    QDBusInterface iface("org.freedesktop.FileManager1",
                         "/org/freedesktop/FileManager1",
                         "org.freedesktop.FileManager1",
                         QDBusConnection::sessionBus());
    // Convert filepath to URI first.
    const QStringList uris = { QUrl::fromLocalFile(path).toString() };

    // StartupId is empty here.
    QDBusPendingCall call = iface.asyncCall("ShowItems", uris, "");
    Q_UNUSED(call);
}

void copyImageToClipboard(const QStringList &paths)
{
    //  Get clipboard
    QClipboard *cb = QApplication::clipboard();

    // Ownership of the new data is transferred to the clipboard.
    QMimeData* newMimeData = new QMimeData();

    // Copy old mimedata
    const QMimeData* oldMimeData = cb->mimeData();
    for ( const QString &f : oldMimeData->formats())
        newMimeData->setData(f, oldMimeData->data(f));

    // Copy file (gnome)
    QByteArray gnomeFormat = QByteArray("copy\n");
    for (QString path : paths) {
        gnomeFormat.append(QUrl::fromLocalFile(path).toEncoded()).append("\n");
    }
    gnomeFormat.remove(gnomeFormat.length() - 1, 1);
    newMimeData->setData("x-special/gnome-copied-files", gnomeFormat);

    // Set the mimedata
    cb->setMimeData(newMimeData);
}

QString getFileContent(const QString &file) {
    QFile f(file);
    QString fileContent = "";
    if (f.open(QFile::ReadOnly))
    {
        fileContent = QLatin1String(f.readAll());
        f.close();
    }
    return fileContent;
}

bool writeTextFile(QString filePath, QString content) {
    QFile file(filePath);
    if (file.open(QIODevice::ReadWrite|QIODevice::Text)) {
        QTextStream in(&file);
        in << content << endl;
        file.close();
        return true;
    }

    return false;
}

bool trashFile(const QString &file)
{
#ifdef QT_GUI_LIB
    QString trashPath;
    QString trashInfoPath;
    QString trashFilesPath;

    QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    // There maby others location for trash like $HOME/.trash or
    // $XDG_DATA_HOME/Trash, but our stupid FileManager coder said we should
    // assume that the trash lcation is $HOME/.local/share/Trash,so...
    trashPath = home + "/.local/share/Trash";
    trashInfoPath = trashPath + "/info";
    trashFilesPath = trashPath + "/files";
    if (! QDir(trashFilesPath).exists()) {
        QDir().mkpath(trashFilesPath);
    }
    if (! QDir(trashInfoPath).exists()) {
        QDir().mkpath(trashInfoPath);
    }

    QFileInfo originalInfo( file );
    if(! originalInfo.exists()) {
        qWarning() << "File doesnt exists, cant move to trash";
        return false;
    }
    // Info for restore
    QString infoStr;
    infoStr += "[Trash Info]\nPath=";
    infoStr += originalInfo.absoluteFilePath();
    infoStr += "\nDeletionDate=";
    infoStr += QDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ss.zzzZ");
    infoStr += "\n";

    QString trashname = originalInfo.fileName();
    QString infopath = trashInfoPath + "/" + trashname + ".trashinfo";
    QString filepath = trashFilesPath + "/" + trashname;
    int nr = 1;
    while( QFileInfo( infopath ).exists() || QFileInfo( filepath ).exists() ){
        nr++;
        trashname = originalInfo.baseName() + "." + QString::number( nr );
        if( !originalInfo.completeSuffix().isEmpty() ){
            trashname += QString( "." ) + originalInfo.completeSuffix();
        }
        infopath = trashInfoPath + "/" + trashname + ".trashinfo";
        filepath = trashFilesPath + "/" + trashname;
    }
    QFile infoFile(infopath);
    if (infoFile.open(QIODevice::WriteOnly)) {
        infoFile.write(infoStr.toUtf8());
        infoFile.close();

        if( !QDir().rename( originalInfo.absoluteFilePath(), filepath ) ){
            qWarning() << "move to trash failed!";
            return false;
        }
    }
    else {
        qDebug() << "Move to trash failed! Could not write *.trashinfo!";
        return false;
    }
    return true;
#else
    Q_UNUSED( file );
    qWarning() << "Trash in server-mode not supported";
    return false;
#endif
}

}  // namespace base

}  // namespace utils
