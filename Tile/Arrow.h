#include "Tile.h"
#include "Theme.h"

/*
Copyright � 2011 Rick Parrish
*/

#pragma once

namespace Tiles
{

struct Arrow : public Tile
{
  	Arrow(identity_t id, Theme& theme, orient_t flow);
	virtual ~Arrow() { };
    /// <param name="canvas">Canvas where text will appear.</param>
	virtual bool Draw(ICanvas *canvas, bool bFocus);
	// get/set accessors for changed flag.
	using Tile::getChanged;
	using Tile::setChanged;
	// set callback for change notification.
	using Tile::watch;

	using Tile::identity;
	// instance type
	virtual const char* getType() const;
	using Tile::getMin;
	using Tile::setMin;
	// get/set accessors for layout maximums
	using Tile::getMax;
	using Tile::setMax;
	// get/set accessors for layout weights
	using Tile::getWeight;
	using Tile::setWeight;
	// get/set accessors for layout descriptors
	using Tile::getFlow;
	using Tile::setFlow;
	// get/set accessor for bounding rectangle.
	using Tile::setRect;
	using Tile::getRect;
	// accessors for scrollable area.
	using Tile::getScrollBox;
	using Tile::setScrollBox;
	using Tile::getScrollArea;
	using Tile::setScrollArea;
	using Tile::getScrollPoint;
	using Tile::setScrollPoint;

	using Tile::contains;
	static const char* type();

	const string_t &getText() const;

	// PickColor needs access to these.
	Theme::Color _fore;
	Theme::Color _back;
	// orientation of arrow
	orient_t _flow;

private:
	// glyph index
	string_t _text;
};

};