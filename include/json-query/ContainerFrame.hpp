#pragma once

#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>

namespace json_query {

// Lightweight frame used by JSONPath recursive-descent traversal.
// Stores owning copies of the parent container (implicit-shared in Qt)
// and an iterator/index pointing at the next child to visit.
struct ContainerFrame
{
    QJsonObject object;
    QJsonArray  array;
    QJsonObject::const_iterator objIt;
    int arrIndex = -1;

    explicit ContainerFrame(const QJsonObject &o)
        : object(o), objIt(object.begin()) {}
    explicit ContainerFrame(const QJsonArray &a)
        : array(a), arrIndex(0) {}

    bool hasNext() const
    {
        return (!object.isEmpty() && objIt != object.end()) ||
               (!array.isEmpty() && arrIndex < array.size());
    }

    QJsonValue next()
    {
        if (!object.isEmpty())
        {
            QJsonValue ref = objIt.value();
            ++objIt;
            return ref;
        }
        QJsonValue ref = array.at(arrIndex);
        ++arrIndex;
        return ref;
    }
};

} // namespace json_query
