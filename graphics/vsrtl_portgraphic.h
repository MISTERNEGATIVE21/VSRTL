#ifndef VSRTL_PORTGRAPHIC_H
#define VSRTL_PORTGRAPHIC_H

#include "vsrtl_graphics_defines.h"
#include "vsrtl_graphicsbase.h"
#include "vsrtl_wiregraphic.h"

#include <QFont>
#include <QPen>

namespace vsrtl {

class Port;

enum class PortType { in, out };

class PortGraphic : public GraphicsBase {
public:
    PortGraphic(Port* port, PortType type, QGraphicsItem* parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* item, QWidget*) override;
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;
    void mousePressEvent(QGraphicsSceneMouseEvent*) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent*) override;
    void updateGeometry();
    Port* getPort() const { return m_port; }
    void setInputWire(WireGraphic* wire);
    void updateInputWire();
    void updateWireGeometry();

    QPointF getInputPoint() const;
    QPointF getOutputPoint() const;

    const QPen& getPen();

private:
    void propagateRedraw();
    void updatePen(bool aboutToBeSelected = false, bool aboutToBeDeselected = false);
    void updateSlot();
    void initializeSignals();

    // m_selected: does not indicate visual selection (ie. isSelected()), but rather whether any port in the port/wire
    // connection of this port has been selected.
    bool m_selected = false;
    bool m_showValue = false;
    ValueDisplayFormat m_valueBase = ValueDisplayFormat::baseTen;

    QRectF m_boundingRect;
    QRectF m_innerRect;

    PortType m_type;
    Port* m_port;

    WireGraphic* m_outputWire = nullptr;
    WireGraphic* m_inputWire = nullptr;

    QString m_widthText;
    QFont m_font;
    QPen m_pen;
    QPen m_oldPen;  // Pen which was previously used for paint(). If a change between m_oldPen and m_pen is seen, this
                    // triggers redrawing of the connected wires
};

}  // namespace vsrtl

#endif  // VSRTL_PORTGRAPHIC_H
