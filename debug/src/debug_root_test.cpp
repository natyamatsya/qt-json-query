#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include "json-query/json-path/JSONPath.hpp"

using json_query::JSONPath;

int main() {
    // Create the same sample document as in the test
    QJsonObject bicycle{{"color","red"},{"price",19.95}};
    QJsonArray books{
        QJsonObject{{"category","reference"},{"author","Nigel Rees"},{"title","Sayings of the Century"},{"price",8.95}},
        QJsonObject{{"category","fiction"},{"author","Evelyn Waugh"},{"title","Sword of Honour"},{"price",12.99}},
        QJsonObject{{"category","fiction"},{"author","Herman Melville"},{"title","Moby Dick"},{"isbn","0-553-21311-3"},{"price",8.99}},
        QJsonObject{{"category","fiction"},{"author","J. R. R. Tolkien"},{"title","The Lord of the Rings"},{"isbn","0-395-19395-8"},{"price",22.99}}
    };
    QJsonObject store{{"book",books},{"bicycle",bicycle}};
    QJsonObject root{{"store",store},{"expensive",10}};
    QJsonDocument doc(root);
    
    qDebug() << "Original root object:" << QJsonDocument(root).toJson(QJsonDocument::Compact);
    
    // Test root path evaluation
    auto jp = JSONPath::create(QString("$"));
    if (jp) {
        auto result = jp->evaluate(doc);
        if (result) {
            if (result->isObject()) {
                qDebug() << "Root path result (object):" << QJsonDocument(result->toObject()).toJson(QJsonDocument::Compact);
            } else if (result->isArray()) {
                qDebug() << "Root path result (array):" << QJsonDocument(result->toArray()).toJson(QJsonDocument::Compact);
            } else {
                qDebug() << "Root path result (other):" << *result;
            }
            qDebug() << "Result type:" << result->type();
        } else {
            qDebug() << "Root path evaluation failed";
        }
    } else {
        qDebug() << "Failed to create JSONPath for '$'";
    }
    
    return 0;
}
