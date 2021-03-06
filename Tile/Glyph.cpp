#include "stdafx.h"
#include "Glyph.h"
#include "Pane.h"
#include "ICanvas.h"
#include "JSON.h"

/*
Copyright � 2011, 2012 Rick Parrish
*/

using namespace Tiles;

Glyph::Glyph(identity_t id, Theme& theme, Theme::Font& font, const TCHAR *text) :
	Tile(id, theme, font)
{
	_text = text;
	Flow desc = {1, 1, 0, true};
	setFlow(eRight, desc);
	setFlow(eDown, desc);
}

// instance type
const char* Glyph::getType() const
{
	return type();
}

const char* Glyph::type()
{
	return "Glyph";
}

/// <param name="canvas">Canvas where text will appear.</param>
bool Glyph::Draw(ICanvas *canvas, bool bFocus)
{
	bFocus; // ignored.
	align_t align = eLeft|eRight|eUp|eDown;
	const Font& font = getFont(_font);
	canvas->DrawString(_rect, _scrollBox, _theme.CaptionFore, _theme.CaptionBack, font, align, _text);
	_bChanged = false;
	return true;
}

// serialize
bool Glyph::save(JSON::Writer &writer)
{
	writer.writeStartObject();
	writer.writeNamedValue("type", type());
	Tile::save(writer);
	saveFont(writer, "Font", _font, false);
	writer.writeEndObject(true);
	return true;
}
