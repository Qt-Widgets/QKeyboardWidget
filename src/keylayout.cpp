#include "keylayout.h"

#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

KeyLayout::KeyLayout(const QString &json, QObject *parent) : QObject(parent)
{
    Q_ASSERT(!json.isNull() && !json.isEmpty());
    QJsonParseError error{};
    jsonObject = QJsonDocument::fromJson(json.toUtf8(), &error).object();

    if (Q_UNLIKELY(error.error != 0)) qDebug() << error.errorString();

    this->initLayouts();
}

KeyLayout::KeyLayout(QFile *file, QObject *parent) : QObject(parent)
{

    QJsonParseError error{};
    file->open(QIODevice::ReadOnly | QIODevice::Text);
    qDebug() << file->isReadable();
    // Could happen that you open it and its not readable and the assert breaks without closing.
    // This hack prevents that with the same goal.
    if (!file->isReadable()) Q_ASSERT(0);

    QByteArray json = file->readAll();
    jsonObject = QJsonDocument::fromJson(json, &error).object();
    file->close();

    if (Q_UNLIKELY(error.error != 0)) qDebug() << error.errorString();

    this->initLayouts();
}

QString KeyLayout::getLocale()
{
    return mLocale;
}

const QVector<QVector<QVector<Key>>> &KeyLayout::getLayouts()
{
    return layouts;
}

QVector<QVector<Key>> *KeyLayout::getRows(char layout)
{
    return &layouts[layout];
}

QJsonValue KeyLayout::getQJsonValue(const QJsonObject &obj, const QString &key, Type type)
{
    Q_ASSERT(!obj.isEmpty());
    QJsonValue value = obj.value(key);
    switch (type) {
        case Type::Array:
            Q_ASSERT(value.isArray());
            break;
        case Type::String:
            Q_ASSERT(value.isString());
            break;
        default:
            return value;
    }
    return value;
}

void KeyLayout::initLayouts()
{

    mLocale = jsonObject.value(QString("locale")).toString();
    mWidth = getQJsonValue(jsonObject, "width", Type::Null).toString().toInt();
    mHeight = getQJsonValue(jsonObject, "height", Type::Null).toString().toInt();

    QJsonValue value = getQJsonValue(jsonObject, "layouts", Type::Array);
    QJsonArray layouts = value.toArray();

    foreach (const QJsonValue &val, layouts) {
        QJsonObject jsonObj = val.toObject();
        layoutNames.append(getQJsonValue(jsonObj, "name", Type::String).toString());

        QJsonValue kbdsVal = getQJsonValue(jsonObj, "keys", Type::Array);

        // initialize keys modifiers
        initModKeys(getQJsonValue(jsonObj, "modifiers", Type::Array).toArray());

        // initialize rows
        this->layouts.append(this->initRows(kbdsVal.toArray()));
    }
}

void KeyLayout::initModKeys(const QJsonArray &modKeysArray)
{

    foreach (const QJsonValue &modType, modKeysArray) {
        QJsonObject jsonObj = modType.toObject();
        qDebug() << jsonObj.contains("modkey") << " " << jsonObj.contains("rewidth");
        if (jsonObj.contains("modkey")) {
            modKeys.insert(getQJsonValue(modType.toObject(), "modkey", Type::String).toString(),
                           getQJsonValue(modType.toObject(), "switchto", Type::String).toString());
        }

        if (jsonObj.contains("setwidth")) {
            widthKeys.insert(getQJsonValue(modType.toObject(), "setwidth", Type::String).toString(),
                             (getQJsonValue(modType.toObject(), "width", Type::String).toString()).toInt());
        }

        if (jsonObj.contains("seticon")) {
            iconKeys.insert(getQJsonValue(modType.toObject(), "seticon", Type::String).toString(),
                            getQJsonValue(modType.toObject(), "icon", Type::String).toString());
        }
    }
}

QVector<QVector<Key>> KeyLayout::initRows(const QJsonArray &keysArray)
{
    int x = 0;
    int y = 0;
    Key *previouskey = nullptr;
    QVector<QVector<Key>> rows;
    foreach (const QJsonValue &rowKeys, keysArray) {
        qDebug() << "RowKeys: " << rowKeys.toArray();
        QJsonArray array = rowKeys.toArray();

        QVector<Key> keys;
        for (auto it = array.begin(); it != array.end(); ++it) {
            // qDebug() << "Items: " << it->toString();
            QString text = it->toString();
            if (x > 0 && previouskey != nullptr) {
                if (widthKeys.contains(text)) {
                    keys.append(Key(text, widthKeys.value(text), mHeight, previouskey->getX() + previouskey->getWidth(),
                                    y * 26));
                } else {
                    keys.append(Key(text, mWidth, mHeight, previouskey->getX() + previouskey->getWidth(), y * 26));
                }

            } else {
                keys.append(Key(it->toString(), mWidth, mHeight, 0, y * 26));    // TODO: change the hardcoded 26
            }

            if (iconKeys.contains(text)) {
                keys.last().setIconFile(iconKeys.value(text));
            }
            x++;
            previouskey = &keys.last();
        }
        rows.append(keys);

        y++;
        x = 0;
        previouskey = nullptr;
        // qDebug() << "count: " << array.count();
    }
    return rows;
}

bool KeyLayout::isModifier(const QString &keyText)
{
    return modKeys.contains(keyText);
}

char KeyLayout::getLayoutIdxFromKey(const QString &keyText)
{
    // We will never have >2147483647 layouts so no need to alloc memory for nothing
    return static_cast<char>(layoutNames.indexOf(modKeys.value(keyText)));
}
