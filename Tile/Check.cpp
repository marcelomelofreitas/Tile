#include "stdafx.h"
#include "Check.h"
#include <vector>
#include "ICanvas.h"
#include "Pane.h"
#include "JSON.h"

/*
Copyright � 2011, 2012 Rick Parrish
*/

using namespace Tiles;

Check::Check(identity_t id, Theme &theme, IAccessor<bool> *pAccess, align_t align) : 
	Control(id, theme),
	_cursor(0), 
	_checked(false),
	_access(pAccess),
	_align(align)
{
	Theme::Font font = {Theme::eArrow, theme.Arrow};
	_tile.setFont(font);

	Flow desc = {1, 1, 0, true};
	setFlow(eDown, desc);
	setFlow(eRight, desc);
	_glyphs[0].index = Theme::eUncheck;
	_glyphs[0].glyph = theme.Uncheck;
	_glyphs[1].index = Theme::eChecked;
	_glyphs[1].glyph = theme.Checked;
}

Check::Check(identity_t id, Theme &theme, bool &checked, align_t align) : 
	Control(id, theme),
	_cursor(0), 
	_checked(false),
	_access( new Reference<bool>(checked) ),
	_align(align)
{
	Theme::Font font = {Theme::eArrow, theme.Arrow};
	_tile.setFont(font);

	Flow desc = {1, 1, 0, true};
	_tile.setFlow(eDown, desc);
	_tile.setFlow(eRight, desc);
	_glyphs[0].index = Theme::eUncheck;
	_glyphs[0].glyph = theme.Uncheck;
	_glyphs[1].index = Theme::eChecked;
	_glyphs[1].glyph = theme.Checked;
}

/// <param name="canvas">canvas where this object will be drawn</param>
bool Check::Draw(ICanvas *canvas, bool bFocus)
{
	_ASSERT(_access != NULL);
	const Theme &theme = _tile.getTheme();
	_checked = _access->getValue();
	bool focus = bFocus && _focus;
	color_t fore = _foreAccess->getValue(theme, focus);
	color_t back = _backAccess->getValue(theme, focus);
	const string_t &checked = theme.getGlyph(_glyphs[_checked]);
	const Font& font = _tile.getFont(_tile.getFont());
	canvas->DrawString(_tile.rect(), _tile.scrollBox(), fore, back, font, _align, checked);
	_tile.setChanged(false);
	return true;
}

// key event sink
bool Check::dispatch(KeyEvent &action)
{
	// key down
	if ( getEnable() && !getReadOnly() &&
		action._what == KeyEvent::DOWN)
	{
		// space bar
		if (action._code == VK_SPACE)
		{
			if (_access != NULL)
				_checked = _access->getValue();
			_checked = !_checked;
			if (_access != NULL)
				_access->setValue(_checked);
			if (!Select.empty())
				Select(this, _checked);
			setChanged(true);
			return true;
		}
	}
	return false;
}

// mouse event sink
bool Check::dispatch(MouseEvent &action)
{
	if (action._button == MouseEvent::eLeft &&
		action._what == action.eDownClick)
	{
		if ( getEnable() && !getReadOnly() )
		{
			if (_access != NULL)
				_checked = _access->getValue();
			_checked = !_checked;
			if (_access != NULL)
				_access->setValue(_checked);
			if (!Select.empty())
				Select(this, _checked);
			setChanged(true);
		}
		if (!_focus)
			getContainer()->setFocus(this);
	}
	return Control::dispatch(action);
}

// instance type
const char* Check::getType() const
{
	return Check::type();
}

const char* Check::type()
{
	return "Check";
}

// Check box specific methods

void Check::setAccessor(IAccessor<bool> *pAccess)
{
	_access = pAccess;
}

void Check::setGlyphs(const TCHAR *checked, const TCHAR *unchecked)
{
	_glyphs[0].index = Theme::eDefault;
	_glyphs[0].glyph = unchecked;
	_glyphs[1].index = Theme::eDefault;
	_glyphs[1].glyph = checked;
}

void Check::setFore(const Theme::Color &normal, const Theme::Color &focus)
{
	_foreView = normal;
	_foreEdit = focus;
}

void Check::setBack(const Theme::Color &normal, const Theme::Color &focus)
{
	_backView = normal;
	_backEdit = focus;
}

void Check::setAlign(align_t align)
{
	_align = align;
}
