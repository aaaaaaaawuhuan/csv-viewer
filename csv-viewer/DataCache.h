#ifndef DATACACHE_H
#define DATACACHE_H

#include <QObject>
#include <QList>
#include <QStringList>
#include <QHash>

class DataCache : public QObject
{
    Q_OBJECT

public:
    struct CacheEntry {
        int startRow;
        int rowCount;
        QList<QStringList> data;
        qint64 timestamp;
        int accessCount;
        
        CacheEntry() : startRow(0), rowCount(0), timestamp(0), accessCount(0) {}
        CacheEntry(int start, int count, const QList<QStringList>& d) 
            : startRow(start), rowCount(count), data(d), timestamp(0), accessCount(0) {}
    };

    explicit DataCache(QObject *parent = nullptr, int maxSize = 10000);
    
    bool getData(int startRow, int rowCount, QList<QStringList>& result);
    void putData(int startRow, const QList<QStringList>& data);
    void clear();
    int size() const { return m_cache.size(); }
    int maxSize() const { return m_maxSize; }
    void setMaxSize(int maxSize) { m_maxSize = maxSize; }

private:
    QList<CacheEntry> m_cache;
    QHash<QString, int> m_cacheIndex; // 用于快速查找缓存项
    int m_maxSize;
    qint64 getCurrentTimestamp() const;
    
    QString makeKey(int startRow, int rowCount) const;
    void removeOldestEntry();
};

#endif // DATACACHE_H