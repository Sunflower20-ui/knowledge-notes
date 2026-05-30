#include "export_service.h"
#include "note_repository.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QRegularExpression>

ExportService::ExportService(NoteRepository *repo, QObject *parent)
    : QObject(parent), m_repo(repo)
{
}

QString ExportService::exportNote(qint64 noteId, const QString &dirPath)
{
    NoteData note = m_repo->getNote(noteId);
    if (note.id < 0 || note.isDeleted) return {};

    return writeNoteFile(note, dirPath);
}

QStringList ExportService::exportAll(const QString &dirPath)
{
    QStringList result;
    QVector<NoteData> notes = m_repo->listNotes(10000, 0);
    for (const NoteData &n : notes) {
        QString path = writeNoteFile(n, dirPath);
        if (!path.isEmpty()) result.append(path);
    }
    return result;
}

QStringList ExportService::exportFolder(const QString &folder, const QString &dirPath)
{
    QStringList result;
    QVector<NoteData> notes = m_repo->notesForFolder(folder);
    QString subDir = dirPath + QStringLiteral("/") + sanitizeFileName(folder);
    QDir().mkpath(subDir);
    for (const NoteData &n : notes) {
        QString path = writeNoteFile(n, subDir);
        if (!path.isEmpty()) result.append(path);
    }
    return result;
}

QString ExportService::writeNoteFile(const NoteData &note, const QString &dirPath)
{
    QDir().mkpath(dirPath);

    QString fileName = sanitizeFileName(note.title);
    if (fileName.isEmpty()) fileName = QStringLiteral("untitled");
    fileName += QStringLiteral(".md");

    QString filePath = dirPath + QStringLiteral("/") + fileName;

    // Handle duplicate file names
    int counter = 1;
    while (QFileInfo::exists(filePath)) {
        QString altName = sanitizeFileName(note.title);
        if (altName.isEmpty()) altName = QStringLiteral("untitled");
        altName += QStringLiteral("_%1.md").arg(counter++);
        filePath = dirPath + QStringLiteral("/") + altName;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "ExportService: cannot write" << filePath;
        return {};
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream << note.content;
    file.close();

    qInfo() << "Exported:" << filePath;
    return filePath;
}

QString ExportService::sanitizeFileName(const QString &title)
{
    QString name = title.trimmed();
    // Replace characters not allowed in Windows filenames
    static QRegularExpression invalid(QStringLiteral("[\\\\/:*?\"<>|]"));
    name.replace(invalid, QStringLiteral("_"));
    // Collapse multiple underscores / spaces
    name = name.simplified().left(200);
    return name;
}
