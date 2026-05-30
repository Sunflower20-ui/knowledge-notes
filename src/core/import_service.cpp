#include "import_service.h"
#include "note_repository.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QDirIterator>

ImportService::ImportService(NoteRepository *repo, QObject *parent)
    : QObject(parent), m_repo(repo)
{
}

qint64 ImportService::importFile(const QString &filePath, const QString &folder)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "ImportService: cannot open" << filePath;
        return -1;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    QString content = stream.readAll();
    file.close();

    return importOne(filePath, content, folder);
}

int ImportService::importDirectory(const QString &dirPath, const QString &folder)
{
    int count = 0;
    QDirIterator it(dirPath, QStringList() << QStringLiteral("*.md"),
                    QDir::Files, QDirIterator::Subdirectories);

    while (it.hasNext()) {
        it.next();
        QFile file(it.filePath());
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) continue;

        QTextStream stream(&file);
        stream.setEncoding(QStringConverter::Utf8);
        QString content = stream.readAll();
        file.close();

        // Determine sub-folder from relative path
        QString subFolder = folder;
        if (subFolder.isEmpty()) {
            QDir baseDir(dirPath);
            QString relDir = baseDir.relativeFilePath(it.fileInfo().absolutePath());
            if (relDir != QStringLiteral(".") && !relDir.isEmpty())
                subFolder = relDir;
        }

        qint64 id = importOne(it.filePath(), content, subFolder);
        if (id > 0) count++;
    }

    return count;
}

qint64 ImportService::importOne(const QString &filePath, const QString &content,
                                 const QString &folder)
{
    QFileInfo fi(filePath);
    QString title = extractTitle(content, fi.completeBaseName());
    qint64 id = m_repo->createNote(title, content);
    if (id > 0 && !folder.isEmpty())
        m_repo->setNoteFolder(id, folder);
    return id;
}

QString ImportService::extractTitle(const QString &content, const QString &fallback)
{
    // Use first # heading if available
    for (const QString &line : content.split('\n')) {
        QString trimmed = line.trimmed();
        if (trimmed.startsWith(QStringLiteral("# "))) {
            QString t = trimmed.mid(2).trimmed();
            if (!t.isEmpty()) return t;
        }
    }

    // Fallback: first non-empty line without markdown markers
    for (const QString &line : content.split('\n')) {
        QString t = line.trimmed();
        while (t.startsWith('#')) t = t.mid(1).trimmed();
        if (!t.isEmpty()) return t;
    }

    return fallback.isEmpty() ? QStringLiteral("\u672a\u547d\u540d") : fallback;
}
