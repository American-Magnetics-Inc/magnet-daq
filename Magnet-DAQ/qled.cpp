/* This file is part of the KDE libraries
Copyright (C) 1998 J?rg Habenicht (j.habenicht@europemail.com)
Copyright (C) 2010 Christoph Feck <cfeck@kde.org>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public License
along with this library; see the file COPYING.LIB.  If not, write to
the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
Boston, MA 02110-1301, USA.
*/

#include "stdafx.h"
#include "qled.h"

#include <QApplication>
#include <QPainter>
#include <QImage>
#include <QStyle>
#include <QStyleOption>

class KLed::Private
{
public:
	Private()
		: darkFactor(300),
		state(On), look(Raised), shape(Circular)
	{
	}

	int darkFactor;
	QColor color;
	State state;
	Look look;
	Shape shape;

	QPixmap cachedPixmap[2]; // for both states
};

KLed::KLed(QWidget *parent)
	: QWidget(parent),
	d(new Private)
{
	setColor(Qt::green);
	updateAccessibleName();
}

KLed::KLed(const QColor &color, QWidget *parent)
	: QWidget(parent),
	d(new Private)
{
	setColor(color);
	updateAccessibleName();
}

KLed::KLed(const QColor &color, State state, Look look, Shape shape,
	QWidget *parent)
	: QWidget(parent),
	d(new Private)
{
	d->state = (state == Off ? Off : On);
	d->look = look;
	d->shape = shape;

	setColor(color);
	updateAccessibleName();
}

KLed::~KLed()
{
	delete d;
}

KLed::State KLed::state() const
{
	return d->state;
}

KLed::Shape KLed::shape() const
{
	return d->shape;
}

QColor KLed::color() const
{
	return d->color;
}

KLed::Look KLed::look() const
{
	return d->look;
}

void KLed::setState(State state)
{
	if (d->state == state)
	{
		return;
	}

	d->state = (state == Off ? Off : On);
	updateCachedPixmap();
	updateAccessibleName();
}

void KLed::setShape(Shape shape)
{
	if (d->shape == shape)
	{
		return;
	}

	d->shape = shape;
	updateCachedPixmap();
}

void KLed::setColor(const QColor &color)
{
	if (d->color == color)
	{
		return;
	}

	d->color = color;
	updateCachedPixmap();
}

void KLed::setDarkFactor(int darkFactor)
{
	if (d->darkFactor == darkFactor)
	{
		return;
	}

	d->darkFactor = darkFactor;
	updateCachedPixmap();
}

int KLed::darkFactor() const
{
	return d->darkFactor;
}

void KLed::setLook(Look look)
{
	if (d->look == look)
	{
		return;
	}

	d->look = look;
	updateCachedPixmap();
}

void KLed::toggle()
{
	d->state = (d->state == On ? Off : On);
	updateCachedPixmap();
	updateAccessibleName();
}

void KLed::on()
{
	setState(On);
}

void KLed::off()
{
	setState(Off);
}

void KLed::resizeEvent(QResizeEvent *)
{
	updateCachedPixmap();
}

QSize KLed::sizeHint() const
{
	QStyleOption option;
	option.initFrom(this);
	int iconSize = style()->pixelMetric(QStyle::PM_SmallIconSize, &option, this);
	return QSize(iconSize, iconSize);
}

QSize KLed::minimumSizeHint() const
{
	return QSize(16, 16);
}

void KLed::updateAccessibleName()
{
#ifndef QT_NO_ACCESSIBILITY
	QString onName = tr("LED on", "Accessible name of a Led whose state is on");
	QString offName = tr("LED off", "Accessible name of a Led whose state is off");
	QString lastName = accessibleName();

	if (lastName.isEmpty() || lastName == onName || lastName == offName)
	{
		//Accessible name has not been manually set.

		setAccessibleName(d->state == On ? onName : offName);
	}
#endif
}

void KLed::updateCachedPixmap()
{
	d->cachedPixmap[Off] = QPixmap();
	d->cachedPixmap[On] = QPixmap();
	update();
}

void KLed::paintEvent(QPaintEvent *)
{
	if (!d->cachedPixmap[d->state].isNull())
	{
		QPainter painter(this);
		painter.drawPixmap(1, 1, d->cachedPixmap[d->state]);
		return;
	}

	QSize size(width() - 2, height() - 2);
	if (d->shape == Circular)
	{
		// Make sure the LED is round
		const int dim = qMin(width(), height()) - 2;
		size = QSize(dim, dim);
	}
	QPointF center(size.width() / 2.0, size.height() / 2.0);
	const int smallestSize = qMin(size.width(), size.height());
	QPainter painter;

	QImage image(size, QImage::Format_ARGB32_Premultiplied);
	image.fill(0);

	QRadialGradient fillGradient(center, smallestSize / 2.0, QPointF(center.x(), size.height() / 3.0));
	const QColor fillColor = d->state != Off ? d->color : d->color.dark(d->darkFactor);
	fillGradient.setColorAt(0.0, fillColor.light(250));
	fillGradient.setColorAt(0.5, fillColor.light(130));
	fillGradient.setColorAt(1.0, fillColor);

	QConicalGradient borderGradient(center, d->look == Sunken ? 90 : -90);
	QColor borderColor = palette().color(QPalette::Dark);
	if (d->state == On)
	{
		QColor glowOverlay = fillColor;
		glowOverlay.setAlpha(80);

		// This isn't the fastest way, but should be "fast enough".
		// It's also the only safe way to use QPainter::CompositionMode
		QImage img(1, 1, QImage::Format_ARGB32_Premultiplied);
		QPainter p(&img);
		QColor start = borderColor;
		start.setAlpha(255); // opaque
		p.fillRect(0, 0, 1, 1, start);
		p.setCompositionMode(QPainter::CompositionMode_SourceOver);
		p.fillRect(0, 0, 1, 1, glowOverlay);
		p.end();

		borderColor = img.pixel(0, 0);
	}
	borderGradient.setColorAt(0.2, borderColor);
	borderGradient.setColorAt(0.5, palette().color(QPalette::Light));
	borderGradient.setColorAt(0.8, borderColor);

	painter.begin(&image);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setBrush(d->look == Flat ? QBrush(fillColor) : QBrush(fillGradient));
	const QBrush penBrush = (d->look == Flat) ? QBrush(borderColor) : QBrush(borderGradient);
	const qreal penWidth = smallestSize / 8.0;
	painter.setPen(QPen(penBrush, penWidth));
	QRectF r(penWidth / 2.0, penWidth / 2.0, size.width() - penWidth, size.height() - penWidth);
	if (d->shape == Rectangular)
	{
		painter.drawRect(r);
	}
	else
	{
		painter.drawEllipse(r);
	}
	painter.end();

	d->cachedPixmap[d->state] = QPixmap::fromImage(image);
	painter.begin(this);
	painter.drawPixmap(1, 1, d->cachedPixmap[d->state]);
	painter.end();
}