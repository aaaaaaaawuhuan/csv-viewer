#include "DataCache.h"
#include <QDateTime>
#include <QDebug>
#include <algorithm>

DataCache::DataCache(QObject *parent, int maxSize)
    : QObject(parent)
    , m_maxSize(maxSize)
{
}

bool DataCache::getData(int startRow, int rowCount, QList<QStringList>& result)
{
    QString key = makeKey(startRow, rowCount);
    
    // 查找完全匹配的缓存项
    for (int i = 0; i < m_cache.size(); ++i) {
        const CacheEntry& entry = m_cache[i];
        if (entry.startRow == startRow && entry.rowCount == rowCount) {
            result = entry.data;
            // 更新时间戳和访问计数
            m_cache[i].timestamp = getCurrentTimestamp();
            m_cache[i].accessCount++;
            return true;
        }
    }
    
    // 查找包含请求范围的缓存项
    for (int i = 0; i < m_cache.size(); ++i) {
        const CacheEntry& entry = m_cache[i];
        if (entry.startRow <= startRow && 
            (entry.startRow + entry.rowCount) >= (startRow + rowCount)) {
            // 从缓存项中提取所需部分
            int offset = startRow - entry.startRow;
            for (int j = 0; j < rowCount && (offset + j) < entry.data.size(); ++j) {
                result.append(entry.data[offset + j]);
            }
            
            // 更新时间戳和访问计数
            m_cache[i].timestamp = getCurrentTimestamp();
            m_cache[i].accessCount++;
            return !result.isEmpty();
        }
    }
    
    return false;
}

void DataCache::putData(int startRow, const QList<QStringList>& data)
{
    if (data.isEmpty()) {
        return;
    }
    
    // 如果缓存已满，移除最旧的条目
    if (m_cache.size() >= m_maxSize) {
        removeOldestEntry();
    }
    
    // 检查是否已存在相同起始行的条目
    QString key = makeKey(startRow, data.size());
    for (int i = 0; i < m_cache.size(); ++i) {
        if (m_cache[i].startRow == startRow && m_cache[i].rowCount == data.size()) {
            // 更新现有条目而不是添加新条目
            m_cache[i].data = data;
            m_cache[i].timestamp = getCurrentTimestamp();
            m_cache[i].accessCount = 0;
            return;
        }
    }
    
    // 添加新条目
    CacheEntry entry(startRow, data.size(), data);
    entry.timestamp = getCurrentTimestamp();
    m_cache.append(entry);
}

void DataCache::clear()
{
    m_cache.clear();
    m_cacheIndex.clear();
}

QString DataCache::makeKey(int startRow, int rowCount) const
{
    return QString::number(startRow) + "_" + QString::number(rowCount);
}

qint64 DataCache::getCurrentTimestamp() const
{
    return QDateTime::currentMSecsSinceEpoch();
}

void DataCache::removeOldestEntry()
{
    if (m_cache.isEmpty()) {
        return;
    }
    
    // 查找最久未使用的条目（基于时间戳和访问计数）
    int oldestIndex = 0;
    qint64 oldestScore = m_cache[0].timestamp - m_cache[0].accessCount * 1000;
    
    for (int i = 1; i < m_cache.size(); ++i) {
        qint64 score = m_cache[i].timestamp - m_cache[i].accessCount * 1000;
        if (score < oldestScore) {
            oldestScore = score;
            oldestIndex = i;
        }
    }
    
    m_cache.removeAt(oldestIndex);
}