//
//  AttributeRegistry.cpp
//  metavoxels
//
//  Created by Andrzej Kapolka on 12/6/13.
//  Copyright (c) 2013 High Fidelity, Inc. All rights reserved.
//

#include <QScriptEngine>

#include "AttributeRegistry.h"
#include "MetavoxelData.h"

REGISTER_META_OBJECT(QRgbAttribute)
REGISTER_META_OBJECT(SharedObjectAttribute)
REGISTER_META_OBJECT(SharedObjectSetAttribute)
REGISTER_META_OBJECT(SpannerSetAttribute)

AttributeRegistry* AttributeRegistry::getInstance() {
    static AttributeRegistry registry;
    return &registry;
}

AttributeRegistry::AttributeRegistry() :
    _guideAttribute(registerAttribute(new SharedObjectAttribute("guide", &MetavoxelGuide::staticMetaObject,
        SharedObjectPointer(new DefaultMetavoxelGuide())))),
    _spannersAttribute(registerAttribute(new SpannerSetAttribute("spanners", &Spanner::staticMetaObject))),
    _colorAttribute(registerAttribute(new QRgbAttribute("color"))),
    _normalAttribute(registerAttribute(new QRgbAttribute("normal", qRgb(0, 127, 0)))) {
}

static QScriptValue qDebugFunction(QScriptContext* context, QScriptEngine* engine) {
    QDebug debug = qDebug();
    
    for (int i = 0; i < context->argumentCount(); i++) {
        debug << context->argument(i).toString();
    }
    
    return QScriptValue();
}

void AttributeRegistry::configureScriptEngine(QScriptEngine* engine) {
    QScriptValue registry = engine->newObject();
    registry.setProperty("colorAttribute", engine->newQObject(_colorAttribute.data()));
    registry.setProperty("normalAttribute", engine->newQObject(_normalAttribute.data()));
    registry.setProperty("getAttribute", engine->newFunction(getAttribute, 1));
    engine->globalObject().setProperty("AttributeRegistry", registry);
    engine->globalObject().setProperty("qDebug", engine->newFunction(qDebugFunction, 1));
}

AttributePointer AttributeRegistry::registerAttribute(AttributePointer attribute) {
    if (!attribute) {
        return attribute;
    }
    AttributePointer& pointer = _attributes[attribute->getName()];
    if (!pointer) {
        pointer = attribute;
    }
    return pointer;
}

void AttributeRegistry::deregisterAttribute(const QString& name) {
    _attributes.remove(name);
}

QScriptValue AttributeRegistry::getAttribute(QScriptContext* context, QScriptEngine* engine) {
    return engine->newQObject(getInstance()->getAttribute(context->argument(0).toString()).data(), QScriptEngine::QtOwnership,
        QScriptEngine::PreferExistingWrapperObject);
}

AttributeValue::AttributeValue(const AttributePointer& attribute) :
    _attribute(attribute), _value(attribute ? attribute->getDefaultValue() : NULL) {
}

AttributeValue::AttributeValue(const AttributePointer& attribute, void* value) :
    _attribute(attribute), _value(value) {
}

void* AttributeValue::copy() const {
    return _attribute->create(_value);
}

bool AttributeValue::isDefault() const {
    return !_attribute || _attribute->equal(_value, _attribute->getDefaultValue());
}

bool AttributeValue::operator==(const AttributeValue& other) const {
    return _attribute == other._attribute && (!_attribute || _attribute->equal(_value, other._value));
}

bool AttributeValue::operator==(void* other) const {
    return _attribute && _attribute->equal(_value, other);
}

bool AttributeValue::operator!=(const AttributeValue& other) const {
    return _attribute != other._attribute || (_attribute && !_attribute->equal(_value, other._value));
}

bool AttributeValue::operator!=(void* other) const {
    return !_attribute || !_attribute->equal(_value, other);
}

OwnedAttributeValue::OwnedAttributeValue(const AttributePointer& attribute, void* value) :
    AttributeValue(attribute, value) {
}

OwnedAttributeValue::OwnedAttributeValue(const AttributePointer& attribute) :
    AttributeValue(attribute, attribute ? attribute->create() : NULL) {
}

OwnedAttributeValue::OwnedAttributeValue(const AttributeValue& other) :
    AttributeValue(other.getAttribute(), other.getAttribute() ? other.copy() : NULL) {
}

OwnedAttributeValue::OwnedAttributeValue(const OwnedAttributeValue& other) :
    AttributeValue(other.getAttribute(), other.getAttribute() ? other.copy() : NULL) {
}

OwnedAttributeValue::~OwnedAttributeValue() {
    if (_attribute) {
        _attribute->destroy(_value);
    }
}

OwnedAttributeValue& OwnedAttributeValue::operator=(const AttributeValue& other) {
    if (_attribute) {
        _attribute->destroy(_value);
    }
    if ((_attribute = other.getAttribute())) {
        _value = other.copy();
    }
    return *this;
}

OwnedAttributeValue& OwnedAttributeValue::operator=(const OwnedAttributeValue& other) {
    if (_attribute) {
        _attribute->destroy(_value);
    }
    if ((_attribute = other.getAttribute())) {
        _value = other.copy();
    }
    return *this;
}

Attribute::Attribute(const QString& name) {
    setObjectName(name);
}

Attribute::~Attribute() {
}

void Attribute::read(MetavoxelData& data, MetavoxelStreamState& state) {
    data.createRoot(state.attribute)->read(state);
}

void Attribute::write(const MetavoxelNode& root, MetavoxelStreamState& state) {
    root.write(state);
}

void Attribute::readDelta(MetavoxelData& data, const MetavoxelNode& reference, MetavoxelStreamState& state) {
    data.createRoot(state.attribute)->readDelta(reference, state);
}

void Attribute::writeDelta(const MetavoxelNode& root, const MetavoxelNode& reference, MetavoxelStreamState& state) {
    root.writeDelta(reference, state);
}

QRgbAttribute::QRgbAttribute(const QString& name, QRgb defaultValue) :
    InlineAttribute<QRgb>(name, defaultValue) {
}
 
bool QRgbAttribute::merge(void*& parent, void* children[]) const {
    QRgb firstValue = decodeInline<QRgb>(children[0]);
    int totalRed = qRed(firstValue);
    int totalGreen = qGreen(firstValue);
    int totalBlue = qBlue(firstValue);
    int totalAlpha = qAlpha(firstValue);
    bool allChildrenEqual = true;
    for (int i = 1; i < Attribute::MERGE_COUNT; i++) {
        QRgb value = decodeInline<QRgb>(children[i]);
        totalRed += qRed(value);
        totalGreen += qGreen(value);
        totalBlue += qBlue(value);
        totalAlpha += qAlpha(value);
        allChildrenEqual &= (firstValue == value);
    }
    parent = encodeInline(qRgba(totalRed / MERGE_COUNT, totalGreen / MERGE_COUNT,
        totalBlue / MERGE_COUNT, totalAlpha / MERGE_COUNT));
    return allChildrenEqual;
} 

void* QRgbAttribute::createFromScript(const QScriptValue& value, QScriptEngine* engine) const {
    return encodeInline((QRgb)value.toUInt32());
}

void* QRgbAttribute::createFromVariant(const QVariant& value) const {
    switch (value.userType()) {
        case QMetaType::QColor:
            return encodeInline(value.value<QColor>().rgba());
        
        default:
            return encodeInline((QRgb)value.toUInt());
    } 
}

QWidget* QRgbAttribute::createEditor(QWidget* parent) const {
    QColorEditor* editor = new QColorEditor(parent);
    editor->setColor(QColor::fromRgba(_defaultValue));
    return editor;
}

SharedObjectAttribute::SharedObjectAttribute(const QString& name, const QMetaObject* metaObject,
        const SharedObjectPointer& defaultValue) :
    InlineAttribute<SharedObjectPointer>(name, defaultValue),
    _metaObject(metaObject) {
    
}

void SharedObjectAttribute::read(Bitstream& in, void*& value, bool isLeaf) const {
    if (isLeaf) {
        in >> *((SharedObjectPointer*)&value);
    }
}

void SharedObjectAttribute::write(Bitstream& out, void* value, bool isLeaf) const {
    if (isLeaf) {
        out << decodeInline<SharedObjectPointer>(value);
    }
}

bool SharedObjectAttribute::merge(void*& parent, void* children[]) const {
    SharedObjectPointer firstChild = decodeInline<SharedObjectPointer>(children[0]);
    for (int i = 1; i < MERGE_COUNT; i++) {
        if (firstChild != decodeInline<SharedObjectPointer>(children[i])) {
            *(SharedObjectPointer*)&parent = _defaultValue;
            return false;
        }
    }
    *(SharedObjectPointer*)&parent = firstChild;
    return true;
}

void* SharedObjectAttribute::createFromVariant(const QVariant& value) const {
    return create(encodeInline(value.value<SharedObjectPointer>()));
}

QWidget* SharedObjectAttribute::createEditor(QWidget* parent) const {
    SharedObjectEditor* editor = new SharedObjectEditor(_metaObject, parent);
    editor->setObject(_defaultValue);
    return editor;
}

SharedObjectSetAttribute::SharedObjectSetAttribute(const QString& name, const QMetaObject* metaObject) :
    InlineAttribute<SharedObjectSet>(name),
    _metaObject(metaObject) {
}

void SharedObjectSetAttribute::read(Bitstream& in, void*& value, bool isLeaf) const {
    in >> *((SharedObjectSet*)&value);
}

void SharedObjectSetAttribute::write(Bitstream& out, void* value, bool isLeaf) const {
    out << decodeInline<SharedObjectSet>(value);
}

bool SharedObjectSetAttribute::merge(void*& parent, void* children[]) const {
    for (int i = 0; i < MERGE_COUNT; i++) {
        if (!decodeInline<SharedObjectSet>(children[i]).isEmpty()) {
            return false;
        }
    }
    return true;
}

QWidget* SharedObjectSetAttribute::createEditor(QWidget* parent) const {
    return new SharedObjectEditor(_metaObject, parent);
}

SpannerSetAttribute::SpannerSetAttribute(const QString& name, const QMetaObject* metaObject) :
    SharedObjectSetAttribute(name, metaObject) {
}

void SpannerSetAttribute::read(MetavoxelData& data, MetavoxelStreamState& state) {
    forever {
        SharedObjectPointer object;
        state.stream >> object;
        if (!object) {
            break;
        }
        data.insert(state.attribute, object);
    }
}

void SpannerSetAttribute::write(const MetavoxelNode& root, MetavoxelStreamState& state) {
    Spanner::incrementVisit();
    root.writeSpanners(state);
    state.stream << SharedObjectPointer();
}

void SpannerSetAttribute::readDelta(MetavoxelData& data, const MetavoxelNode& reference, MetavoxelStreamState& state) {
    forever {
        SharedObjectPointer object;
        state.stream >> object;
        if (!object) {
            break;
        }
        data.toggle(state.attribute, object);
    }
}

void SpannerSetAttribute::writeDelta(const MetavoxelNode& root, const MetavoxelNode& reference, MetavoxelStreamState& state) {
    Spanner::incrementVisit();
    root.writeSpannerDelta(reference, state);
    state.stream << SharedObjectPointer();
}
