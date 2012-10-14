#include "stdafx.h"
#include "Flow.h"
#include "../JSON/Writer.h"
#include "ICanvas.h"
#include "IWindow.h"

/*
Copyright � 2011 Rick Parrish
*/

using namespace Tiles;

Flow::Flow(identity_t id, Theme &theme, orient_t flow) : 
	_flow(flow),
	_pDesktop(NULL),
	_tile(id, theme),
	_pNotify(NULL),
	_index(0),
	_shared(0),
	_readonly(false),
	_local(true),
	_scroll(false),
	_hover(0)
{
	_space.local = false;
	_space.thick = theme.GridThick;
	_thick.local = true;
	_thick.thick = 0;
	_border.index = Theme::eDefault;
	_border.color = 0;
	_tile._pSelf = this;
	_tile.setMin(eRight, 0);
	_tile.setMax(eRight, 4096);
	_tile.setMin(eDown, 0);
	_tile.setMax(eDown, 4096);
	_lines.index = Theme::eGridLines;
	_lines.color = RGB(0,0,0);
}

Flow::Flow(identity_t id, Theme &theme, Theme::Font& desc, orient_t flow) : 
	_flow(flow),
	_pDesktop(NULL),
	_tile(id, theme, desc),
	_pNotify(NULL),
	_index(0),
	_shared(0),
	_readonly(false),
	_local(true),
	_scroll(false),
	_hover(0)
{
	_space.local = false;
	_space.thick = theme.GridThick;
	_thick.local = true;
	_thick.thick = 0;
	_border.index = Theme::eDefault;
	_border.color = 0;
	_tile._pSelf = this;
	_tile.setMin(eRight, 0);
	_tile.setMax(eRight, 4096);
	_tile.setMin(eDown, 0);
	_tile.setMax(eDown, 4096);
	_lines.index = Theme::eGridLines;
	_lines.color = RGB(0,0,0);
}

Flow::~Flow()
{
	_map.clear();
	_listControls.clear();
	size_t i = _listTiles.size();
	while (i > 0)
	{
		delete _listTiles[--i];
	}
	_listTiles.clear();
}

void Flow::clear()
{
	_listTiles.clear();
	_listControls.clear();
}

void Flow::reflow()
{
#if 1
	meter_t border = _tile.getTheme().getThick(_thick);
	rect_t rect = {0};
	rect_t box = {0};
	_tile.getRect(rect);
	_tile.getScrollBox(box);

	box.wide -= 2*border;
	box.high -= 2*border;

	rect.x += border;
	rect.y += border;
	if (rect.wide > 2*border)
		rect.wide -= 2*border;
	else
		rect.wide = 0;

	if (rect.high > 2*border)
		rect.high -= 2*border;
	else
		rect.high = 0;
	layout(rect, box);
#else
	meter_t space = _tile.getTheme().getThick(_space);
	meter_t border = _tile.getTheme().getThick(_thick);
	size_t size = _listTiles.size();
	FlowDesc flow = {0};
	rect_t rect = {0};
	_tile.getRect(rect);
	rect_t work = rect;

	getMin(_flow, flow._min);
	getMax(_flow, flow._max);
	for (size_t i = 0; i < size; i++)
	{
		meter_t weight = 0;
		ITile* pTile = _listTiles[i];
		pTile->getWeight(_flow, weight);
		_ASSERT(weight >= 0);
		flow._weight += weight;
	}

	if ( horizontal(_flow) )
	{
		meter_t slack = rect.wide - flow._min;
		if (slack < 0) slack = 0;
		work.x += border;
		work.y += border;
		work.high -= 2*border;
		for (size_t i = 0; i < size; i++)
		{
			ITile* pTile = _listTiles[i];
			FlowDesc desc = {0};
			pTile->getMin(_flow, desc._min);
			pTile->getMax(_flow, desc._max);
			pTile->getWeight(_flow, desc._weight);
			work.wide = desc._min;
			if (flow._weight > 0)
			{
				meter_t grow = (slack * desc._weight) / flow._weight;
				if (grow > desc._max)
					grow = desc._max;
				work.wide += grow;
				slack -= grow;
			}
			flow._weight -= desc._weight;
			meter_t right = rect.right() - border;
			if ( (work.wide + work.x) > right )
				work.wide = right - work.x;
			pTile->setRect(work);
			work.x += work.wide;
			work.x += space;
			if (work.x > right)
				work.x = right;
		}
	}
	else if ( vertical(_flow) )
	{
		work.high = 0;

		meter_t slack = rect.high - flow._min;
		if (slack < 0) slack = 0;
		work.y += border;
		work.x += border;
		work.wide -= 2*border;
		for (size_t i = 0; i < size; i++)
		{
			ITile* pTile = _listTiles[i];
			FlowDesc desc = {0};
			pTile->getMin(_flow, desc._min);
			pTile->getMax(_flow, desc._max);
			pTile->getWeight(_flow, desc._weight);
			work.high = desc._min;
			if (flow._weight > 0)
			{
				meter_t grow = (slack * desc._weight) / flow._weight;
				_ASSERT(grow >= 0);
				if (grow > desc._max)
					grow = desc._max;
				work.high += grow;
				slack -= grow;
			}
			flow._weight -= desc._weight;
			meter_t bottom = rect.bottom() - border;
			if ( (work.high + work.y) > bottom)
				work.high = bottom - work.y;
			pTile->setRect(work);
			work.y += work.high;
			work.y += space;
			if (work.y > bottom)
				work.y = bottom;
		}
	}
#endif
	setChanged(true);
}

// add layout item to the collection.
size_t Flow::Add(ITile *pTile, meter_t min, meter_t max, meter_t weight, bool fontScale)
{
	FlowDesc desc = {min, max, weight, fontScale};
	pTile->setFlow(_flow, desc);
	return Add(pTile);
}

// add layout item to the collection.
size_t Flow::Add(ITile *pTile)
{
	size_t last = _listTiles.size();
	_listTiles.push_back(pTile);
	pTile->watch(this);
	pTile->setContainer(this);
	return last;
}

// add layout item to the collection.
size_t Flow::Add(IControl* pControl, meter_t min, meter_t max, meter_t weight, bool fontScale)
{
	FlowDesc desc = {min, max, weight, fontScale};
	pControl->setFlow(_flow, desc);
	return Add(pControl);
}

// add layout item to the collection.
size_t Flow::Add(IControl* pControl)
{
	size_t last = _listTiles.size();
	_listTiles.push_back(pControl);
	_listControls.push_back(pControl);
	pControl->watch(this);
	pControl->setDesktop(_pDesktop);
	pControl->setContainer(this);
	if (pControl->identity() != 0)
	{
		Flow *pFlow = this;
		while ( !pFlow->isRoot() )
			pFlow = pFlow->getContainer();
		pFlow->_map[pControl->identity()] = pControl;
	}
	return last;
}

// add layout item to the collection.
size_t Flow::Add(Flow* pFlow, meter_t min, meter_t max, meter_t weight, bool fontScale)
{
	IControl *pControl = pFlow;
	std::map<identity_t, IControl*>::iterator it = pFlow->_map.begin();
	while (it != pFlow->_map.end())
	{
		_map[ (*it).first ] = (*it).second;
		it++;
	}
	pFlow->_map.clear();
	pFlow->_local = false;
	return Add(pControl, min, max, weight, fontScale);
}

// add layout item to the collection.
size_t Flow::Add(Flow* pFlow, bool local)
{
	IControl *pControl = pFlow;
	std::map<identity_t, IControl*>::iterator it = pFlow->_map.begin();
	while (it != pFlow->_map.end())
	{
		_map[ (*it).first ] = (*it).second;
		it++;
	}
	pFlow->_map.clear();
	pFlow->_local = local;
	return Add(pControl);
}

orient_t Flow::opposite(orient_t flow)
{
	switch (flow)
	{
		case eLeft:
			flow = eRight;
			break;
		case eRight:
			flow = eLeft;
			break;
		case eUp:
			flow = eDown;
			break;
		case eDown:
			flow = eUp;
			break;
	}
	return flow;
}

// returns true for vertical flows
bool Flow::vertical(orient_t flow)
{
	return flow == eUp || flow == eDown;
}
// returns true for horizontal flows
bool Flow::horizontal(orient_t flow)
{
	return flow == eLeft || flow == eRight;
}

orient_t Flow::getFlow() const
{
	return _flow;
}

const char *Flow::getName(orient_t flow)
{
	switch (flow)
	{
		case eUp:
			return "Up";
		case eDown:
			return "Down";
		case eLeft:
			return "Left";
		case eRight:
			return "Right";
	}
	return "?";
}

// serialize
bool Flow::save(JSON::Writer &writer)
{
	std::string name;
	orient(name, _flow);
	writer.writeStartObject();
	writer.writeNamedValue("type", type());
	writer.writeNamedValue("orient", name.c_str());
	_tile.save(writer);
	writer.writeStartNamedArray("Items");
	// Note: the tiles list includes both tiles and controls so no need to skim the controls list.
	for (std::vector<ITile *>::iterator it = _listTiles.begin(); it != _listTiles.end(); it++)
	{
		ITile *p = *it;
		p->save(writer);
	}
	writer.writeEndArray();
	writer.writeEndObject(true);
	return true;
}

bool Flow::save(const TCHAR *path)
{
	JSON::Writer writer;
	return writer.Open(path) && 
		writer.writeName("Flow") &&
		save(writer) && 
		writer.Close();
}

void Flow::Redraw(ITile* pDraw)
{
	// already marked for change?
	if (_pNotify != NULL && !getChanged())
	{
		// no - pass along.
		_pNotify->Redraw(pDraw);
	}
}

void Flow::watch(IRedraw* pNotify)
{
	_pNotify = pNotify;
	_tile.watch(pNotify);
}

bool Flow::getChanged() const
{
	return _tile.getChanged();
}

void Flow::setChanged(bool bChanged)
{
	_tile.setChanged(bChanged);
}

bool Flow::Draw(ICanvas *canvas, bool bFocus)
{
	rect_t last = {0};
	size_t size = _listTiles.size();
	meter_t space = _tile.getTheme().getThick(_space);
	meter_t border = _tile.getTheme().getThick(_thick);
	for (size_t i = 0; i < size; i++)
	{
		rect_t tile = {0};
		ITile* pDraw = _listTiles[i];
		pDraw->getRect(tile);
		_ASSERT(tile.wide >= 0);
		_ASSERT(tile.high >= 0);
		if (i > 0)
		{
			// is there a gap between horizontal neighbors?
			if ( horizontal(_flow) && space > 0 )
			{
				rect_t fill = tile;
				fill.x -= space;
				fill.wide = space;
				if ( canvas->overlaps(fill) )
					canvas->FillRectangle(fill, _tile.getColor(_lines));
			}
			// is there a gap between vertical neighbors?
			else if ( vertical(_flow) && space > 0 )
			{
				rect_t fill = tile;
				fill.y -= space;
				fill.high = space;
				if ( canvas->overlaps(fill) )
					canvas->FillRectangle(fill, _tile.getColor(_lines));
			}
		}
		//if ( canvas->overlaps(tile) )
			pDraw->Draw(canvas, bFocus);
		last = tile;
	}
	rect_t fill = {0};
	rect_t rect = {0};
	getRect(rect);
	if (size == 0)
		fill = rect;
	else
	{
		fill = last;
		if ( horizontal(_flow) )
		{
			fill.x = fill.right();
			fill.wide = rect.right() - fill.x;
			if ( canvas->overlaps(fill) )
				canvas->FillRectangle(fill, _tile.getColor(_lines));
		}
		else if (vertical(_flow) )
		{
			fill.y = fill.bottom();
			fill.high = rect.bottom() - fill.y;
			if ( canvas->overlaps(fill) )
				canvas->FillRectangle(fill, _tile.getColor(_lines));
		}
	}
	if (border)
	{
		rect_t rect = {0};
		rect_t box = {0};
		getRect(rect);
		getScrollBox(box);
		color_t color = _tile.getColor(_border);
		// top border?
		if (box.y < border)
		{
			rect_t fill = rect;
			fill.high = border - box.y;
			canvas->FillRectangle(fill, color);
		}
		// left border?
		if (box.x < border)
		{
			rect_t fill = rect;
			fill.wide = border - box.x;
			canvas->FillRectangle(fill, color);
		}
		// bottom border?
		if ( (box.y + rect.high) > (box.high - border) )
		{
			rect_t fill = rect;
			fill.high = box.y + rect.high + border - box.high;
			fill.y = rect.y + rect.high - fill.high;
			canvas->FillRectangle(fill, color);
		}
		// right border?
		if ( (box.x + rect.wide) > (box.wide - border) )
		{
			rect_t fill = rect;
			fill.wide = box.x + rect.wide + border - box.wide;
			fill.x = rect.x + rect.wide - fill.wide;
			canvas->FillRectangle(fill, color);
		}
	}
	setChanged(false);
	return true;
}

// ITile implementation
identity_t Flow::identity() const
{
	return _tile.identity();
}

// instance type
const char* Flow::getType() const
{
	return "Flow";
}

const char* Flow::type()
{
	return "Flow";
}

void Flow::getInsideMin(orient_t flow, meter_t &min)
{
	meter_t space = _tile.getTheme().getThick(_space);
	meter_t alt = 0;

	_tile.getMin(flow, min);

	size_t size = _listTiles.size();
	for (size_t i = 0; i < size; i++)
	{
		meter_t tile = 0;
		_listTiles[i]->getMin(flow, tile);
		if ( flow == _flow || flow == opposite(_flow) )
		{
			if (i > 0)
				alt += space;
			alt += tile;
		}
		else if (tile > alt)
		{
			alt = tile;
		}
	}
	if (alt > min)
		min = alt;
}

void Flow::getMin(orient_t flow, meter_t &min)
{
	meter_t border = _tile.getTheme().getThick(_thick);
	getInsideMin(flow, min);
	min += 2*border;
}

void Flow::setMin(orient_t flow, meter_t min)
{
	_tile.setMin(flow, min);
}

void Flow::getMax(orient_t flow, meter_t &max)
{
	meter_t space = _tile.getTheme().getThick(_space);
	meter_t border = _tile.getTheme().getThick(_thick);
	meter_t alt = 0;

	_tile.getMax(flow, max);

	max += 2*border;

	alt = max;

	size_t size = _listTiles.size();
	for (size_t i = 0; i < size; i++)
	{
		meter_t tile = 0;
		_listTiles[i]->getMax(flow, tile);
		if ( flow == _flow || flow == opposite(_flow) )
		{
			if (i > 0)
				alt += space;
			alt += tile;
		}
		else
		{
			tile += 2*border;
			if (tile < alt)
				alt = tile;
		}
	}
	if (alt < max)
		max = alt;
}

void Flow::setMax(orient_t flow, meter_t max)
{
	_tile.setMax(flow, max);
}

// get/set accessors for layout weights
void Flow::getWeight(orient_t flow, meter_t &weight)
{
	_tile.getWeight(flow, weight);
}

void Flow::setWeight(orient_t flow, meter_t weight)
{
	_tile.setWeight(flow, weight);
}

// get/set accessors for layout descriptors
void Flow::getFlow(orient_t flow, FlowDesc &desc)
{
	_tile.getFlow(flow, desc);
}

void Flow::setFlow(orient_t flow, const FlowDesc &desc)
{
	_tile.setFlow(flow, desc);
}

// get/set accessor for bounding rectangle.
void Flow::getRect(rect_t &rect) const
{
	_tile.getRect(rect);
}

void Flow::setRect(const rect_t &rect)
{
	_tile.setRect(rect);
	reflow();
}

// get/set accessor for scroll box - width, height, and (x,y) offset
// where the rect above exposes a portion of the scroll box.
void Flow::getScrollBox(rect_t &box) const
{
	_tile.getScrollBox(box);
}

void Flow::setScrollBox(const rect_t &box)
{
#if 0
	area_t area = box.getArea();
	_tile.setScrollArea(area);
	size_t size = _listTiles.size();
	meter_t space = _tile.getTheme().getThick(_space);
	// assumes bounding rectangle is correct.
	FlowDesc flow = {0};
	rect_t rect = {0};

	getRect(rect);
	getMin(_flow, flow._min);
	getMax(_flow, flow._max);
	for (size_t i = 0; i < size; i++)
	{
		meter_t weight = 0;
		ITile* pTile = _listTiles[i];
		pTile->getWeight(_flow, weight);
		_ASSERT(weight >= 0);
		flow._weight += weight;
	}
	// account for inter-cell spacing.
	if (size > 1)
		flow._min += space * (size - 1);

	if ( horizontal(_flow) )
	{
		meter_t slack = rect.wide - flow._min;
		if (slack < 0) slack = 0;
		for (size_t i = 0; i < _listTiles.size(); i++)
		{
			area_t work = {0};
			ITile* pTile = _listTiles[i];
			FlowDesc desc = {0};
			pTile->getMin(_flow, desc._min);
			pTile->getMax(_flow, desc._max);
			pTile->getWeight(_flow, desc._weight);
			work.wide = desc._min;
			work.high = area.high;
			if (flow._weight > 0)
			{
				meter_t grow = (slack * desc._weight) / flow._weight;
				if (grow > desc._max)
					grow = desc._max;
				work.wide += grow;
				slack -= grow;
			}
			flow._weight -= desc._weight;
			pTile->setScrollArea(work);
		}
	}
	else if ( vertical(_flow) )
	{
		meter_t slack = rect.high - flow._min;
		if (slack < 0) slack = 0;
		for (size_t i = 0; i < _listTiles.size(); i++)
		{
			area_t work = {0};
			ITile* pTile = _listTiles[i];
			FlowDesc desc = {0};
			pTile->getMin(_flow, desc._min);
			pTile->getMax(_flow, desc._max);
			pTile->getWeight(_flow, desc._weight);
			work.high = desc._min;
			work.wide = area.wide;
			if (flow._weight > 0)
			{
				meter_t grow = (slack * desc._weight) / flow._weight;
				if (grow > desc._max)
					grow = desc._max;
				work.high += grow;
				slack -= grow;
			}
			flow._weight -= desc._weight;
			pTile->setScrollArea(work);
		}
	}
	point_t pt = box.getPoint();
	setScrollPoint(pt);
#else
	_tile.setScrollBox(box);
#endif
}

void Flow::getScrollArea(area_t &area) const
{
	_tile.getScrollArea(area);
}

void Flow::setScrollArea(const area_t &area)
{
	_tile.setScrollArea(area);
}

void Flow::getScrollPoint(point_t &pt) const
{
	_tile.getScrollPoint(pt);
}

void Flow::setScrollPoint(const point_t &pt)
{
	_tile.setScrollPoint(pt);
	reflow();
}

// returns true if point lies within layout.
bool Flow::contains(point_t pt)
{
	if ( _tile.contains(pt) )
	{
		for (size_t i = 0; i < _listTiles.size(); i++)
			if (_listTiles[i]->contains(pt) )
				return true;
	}
	return false;
}

// dispatch keypress
// TODO: tidy this up.
bool Flow::dispatch(KeyEvent &action)
{
	if (getIndex() < _listControls.size())
	{
		IControl *pControl = _listControls[getIndex()];
		if ( pControl->dispatch(action) )
			return true;
	}

	if ( !isRoot() )
		return false;

	if (action._what == KeyEvent::DOWN)
	{
		switch (action._code)
		{
			case VK_TAB:
				// SHIFT means reverse tab.
				return onTab( (action._mask & KeyEvent::SHIFT) != 0 );

			case VK_ESCAPE:
				setFocus((IWindow*)NULL);
				return true;

			case VK_LEFT:
				return onNear(eLeft) || onNear(eUp);

			case VK_RIGHT:
				return onFar(eRight) || onFar(eDown);

			case VK_UP:
				return onNear(eUp) || onNear(eLeft);

			case VK_DOWN:
				return onFar(eDown) || onFar(eDown);

			default:
				break;
		}
	}
	return false;
}

// sink all mouse click/move events.
bool Flow::dispatch(MouseEvent &action)
{
	bool bFocus = false;
	size_t size = _listControls.size();
	switch (action._what)
	{
		case MouseEvent::eDownClick:
			bFocus = action._button == MouseEvent::eLeft;
			// fall through!
		case MouseEvent::eUpClick:
			if (getIndex() < size)
			{
				IControl *pControl = _listControls[getIndex()];
				// does this control contain the mouse click?
				if ( pControl->contains(action._place) )
					return pControl->dispatch(action);
			}
			// search all controls: which control contains mouse click?
			for (size_t i = 0; i < size; i++)
			{
				IControl *pControl = _listControls[i];
				// does this control contain the mouse click?
				if ( pControl->contains(action._place) )
				{
					setIndex(i);
					return pControl->dispatch(action);
				}
			}
			break;

		case MouseEvent::eMove:
			// search all controls: which control contains mouse click?
			for (size_t i = 0; i < size; i++)
			{
				IControl* pControl = _listControls[i];
				// does this control contain the mouse click?
				if ( pControl->contains(action._place) )
				{
					return pControl->dispatch(action);
				}
			}
			break;

		default:
			break;
	}
	return false;
}

bool Flow::getFocus() const
{
	size_t size = _listControls.size();
	for (size_t i = 0; i < size; i++)
	{
		IControl *pControl = _listControls[i];
		if (pControl->getFocus())
			return true;
	}
	return false;
}

void Flow::setFocus(bool focus)
{
#if 0
	if (getIndex() < _listControls.size())
	{
		IControl *pControl = _listControls[getIndex()];
		setFocus(pControl);
	}
#else
	focus;
#endif
}

// readonly
bool Flow::getReadOnly() const
{
	return _readonly || !isRoot() && getContainer()->getReadOnly();
}

void Flow::setReadOnly(bool readonly)
{
	_readonly = readonly;
}

void Flow::setHover(bool)
{
}

void Flow::setDesktop(IWindow *pDesktop)
{
	_pDesktop = pDesktop;
	for (size_t i = 0; i < _listControls.size(); i++)
	{
		IControl *pControl = _listControls[i];
		pControl->setDesktop(pDesktop);
	}
}

void Flow::setIndex(size_t index)
{
	size_t size = _listControls.size();
	if (index > size)
		index = size;

	if (getIndex() != index)
	{
		if (_local || isRoot() )
			_index = index;
		else
		{
			Flow *pFlow = getContainer();
			bool changed = pFlow->_shared != index;
			pFlow->_shared = index;
			if (changed)
			{
				// TODO
			}
		}
	}
}

size_t Flow::getIndex() const
{
	if (_local || isRoot() )
		return _index;
	else
		return getContainer()->_shared;
}

// tab key navigation
bool Flow::onTab(bool bReverse)
{
	orient_t goNear = static_cast<orient_t>(eLeft|eUp);
	orient_t goFar = static_cast<orient_t>(eRight|eDown);
	return bReverse ? onNear(goNear) : onFar(goFar);
}

// left/up key navigation
bool Flow::onNear(orient_t flow)
{
	if ( !hasControls() )
		return false;

	size_t size = _listControls.size();
	// aligned with axis of navigation?
	if ( (flow & _flow) || ( flow & opposite(_flow) )  )
	{
		// if we don't have the focus ...
		if ( size > 0 && (getIndex() >= size || !getFocus() ) )
		{
			// we must be entering from afar (right or down) sibling.
			setIndex(size - 1);
		}
		while (getIndex() < size)
		{
			IControl *pControl = _listControls[getIndex()];
			if ( pControl->onNear(flow) )
			{
				break;
			}
			// Walk backwards modulo size plus 1 so
			// zero becomes size which exits the loop.
			setIndex( (getIndex() + size) % (size + 1) );
		}
		return getIndex() < size;
	}
	if (size > 0 && getIndex() >= size) // punt
		setIndex(size - 1);
	if (getIndex() < size)
	{
		IControl *pControl = _listControls[getIndex()];
		if (pControl->hasControls())
			// delegate to descendant.
			return pControl->onNear(flow);
		setIndex( (_index + size) % (size + 1) );
	}
	return false;
}

// right/down key navigation
bool Flow::onFar(orient_t flow)
{
	if ( !hasControls() )
		return false;

	size_t size = _listControls.size();
	// aligned with axis of navigation?
	if ( (flow & _flow) || ( flow & opposite(_flow) )  )
	{
		// if we don't have the focus ...
		if (!getFocus() || getIndex() >= size)
		{
			// we must be entering from a near (left or up) sibling.
			setIndex(0);
		}
		size_t wrap = getIndex();
		// run past end of control list?
		while (getIndex() < size)
		{
			IControl *pControl = _listControls[getIndex()];
			if (pControl->onFar(flow) )
			{
				break;
			}
			// advance index to next control
			setIndex(getIndex() + 1);

			// allow wrap-around on top level only.
			if (getIndex() == size && isRoot())
				setIndex(0);

			// stop looking when we're back where we started.
			if (getIndex() == wrap)
				return false;
		}
		return getIndex() < size;
	}
	if (size > 0 && getIndex() >= size) // punt
		setIndex(0);
	while (getIndex() < size)
	{
		IControl *pControl = _listControls[getIndex()];
		if (pControl->hasControls())
			// delegate to descendant.
			return pControl->onFar(flow);
		else setIndex(getIndex() + 1);
	}
	return false;
}

void Flow::setFocus(IControl *pControl)
{
	if ( isRoot() )
	{
		if (_pDesktop)
			_pDesktop->setFocus(pControl);
	}
	else
		getContainer()->setFocus(pControl);
}

bool Flow::hasControls() const
{
	return _listControls.size() != 0;
}

bool Flow::find(identity_t id, const char *type, IControl *&pControl)
{
	type; // ignored.
	std::map<identity_t, IControl*>::iterator it = _map.find(id);
	if (it != _map.end())
	{
		pControl = (*it).second;
		return true;
	}
	return false;
}

size_t Flow::getControlCount() const
{
	return _listControls.size();
}

IControl* Flow::getControl(size_t index)
{
	if (index < _listControls.size())
		return _listControls[index];
	return NULL;
}

// the tile's containing flow object
Flow *Flow::getContainer() const
{
	return _tile.getContainer();
}

void Flow::setContainer(Flow *pFlow)
{
	_tile.setContainer(pFlow);
}

bool Flow::isRoot() const
{
	return getContainer() == NULL;
}

void Flow::setSpace(meter_t space)
{
	bool changed = _space.thick != space;
	_space.thick = space;
	_space.local = true;
	if (changed && getContainer() != NULL)
	{
		reflow();
	}
}

#if 0
void Flow::Shown(ITile *pShow)
{
	rect_t tile = {0};
	rect_t rect = {0};
	meter_t space = _tile.getTheme().getThick(_space);
	pShow->getScrollBox(tile);
	pShow->getRect(rect);
	// need scroll?
	if (rect.wide < tile.wide || rect.high < tile.high)
	{
		if ( !isRoot() )
			getContainer()->Shown(this);
	}
	size_t size = _listTiles.size();
	size_t iTile = size;
	for (size_t i = 0; i < size; i++)
	{
		if (_listTiles[i] == pShow)
		{
			iTile = i;
			break;
		}
	}
	if (iTile < size)
	{
		// need horizontal scroll?
		if (rect.wide < tile.wide && horizontal(_flow))
		{
			meter_t left = 0;
			meter_t right = 0;
			for (size_t i = 0; i < iTile; i++)
			{
				rect_t peek;
				_listTiles[i]->getScrollBox(peek);
				if (i > 0)
					right += space;
				left = right;
				right += peek.wide;
			}

			// which is closer: left edge or right edge?

			if ( left < tile.x && right < (tile.x + rect.wide) )
			{
				meter_t a = tile.x - left;
				meter_t b = (tile.x + rect.wide) - right;
				if (a < b)
					scroll(a, tile.y);
				else
					scroll(b - tile.wide, tile.y);
			}
			else if ( left > tile.x && right > (tile.x + rect.wide) )
			{
				meter_t a = left - tile.x;
				meter_t b = right - (tile.x + rect.wide);
				if (a < b)
					scroll(a, tile.y);
				else
					scroll(b - tile.wide, tile.y);
			}
			// more ?
		}
		// need vertical scroll?
		if (rect.high < tile.high && vertical(_flow))
		{
			meter_t above = 0;
			meter_t below = 0;
			for (size_t i = 0; i < iTile; i++)
			{
				rect_t peek;
				_listTiles[i]->getScrollBox(peek);
				if (i > 0)
					below += space;
				above = below;
				below += peek.wide;
			}

			// which is closer: left edge or right edge?

			if ( above < tile.y && below < (tile.y + rect.high) )
			{
				meter_t a = tile.y - above;
				meter_t b = (tile.y + rect.high) - below;
				if (a < b)
					scroll(tile.x, a);
				else
					scroll(tile.x, b - tile.high);
			}
			else if ( above > tile.y && below > (tile.y + rect.high) )
			{
				meter_t a = above - tile.y;
				meter_t b = below - (tile.y + rect.high);
				if (a < b)
					scroll(tile.x, a);
				else
					scroll(tile.x, b - tile.high);
			}
			// more ?
		}
	}
}
#endif

void Flow::Shown(ITile *pTile)
{
#if 0
	if ( isRoot() || _scroll)
	{
		rect_t flow = {0};
		rect_t tile = {0};
		rect_t rect = {0};
		Flow *pFlow = getContainer();
		pTile->getScrollBox(tile);
		getScrollBox(flow);
		getRect(rect);
		ITile *walk = pTile;
		while (this != pFlow && pFlow != NULL)
		{
			rect_t box = {0};
			pFlow->getTileBox(walk, box);
			tile.x += box.x;
			tile.y += box.y;
			walk = pFlow;
			pFlow = pFlow->getContainer();
		}
		// scroll it, baby!

		bool bA = flow.x <= tile.x;
		bool bB = (tile.x + tile.wide) <= (flow.x + rect.wide);
		if (tile.wide > rect.wide)
		{
			meter_t a = tile.x - flow.x;
			meter_t b = (tile.x + tile.wide) - (flow.x + rect.wide);
			if (a > b)
				scroll(tile.x, flow.y);
			else
				scroll(tile.x + tile.wide - rect.wide, flow.y);
		}
		else
		{
			if (bA && bB)
			{
				// do nothing. The item is already visible.
			}
			else if (bA)
			{
				meter_t a = tile.x - flow.x;
				meter_t b = (tile.x + tile.wide) - (flow.x + rect.wide);
				if (a > b)
					scroll(tile.x, flow.y);
				else
					scroll(tile.x + tile.wide - rect.wide, flow.y);
			}
			else if (bB)
			{
				scroll(tile.x, flow.y);
			}
			else
			{
			}
		}
	}
	else
	{
		getContainer()->Shown(pTile);
	}
#else
	rect_t box = {0};
	getTileBox(pTile, box);
	box.x = 0;
	box.y = 0;
	balance(box);
#endif
}

void Flow::getTileBox(ITile *pTile, rect_t &box)
{
	meter_t head = 0;
	meter_t tail = 0;
	meter_t space = _tile.getTheme().getThick(_space);
	for (size_t i = 0; i < _listTiles.size(); i++)
	{
		rect_t tile = {0};
		_listTiles[i]->getScrollBox(tile);

		if (i > 0)
			head += space;

		tail = head;

		if (horizontal(_flow))
			head += tile.wide;
		else if (vertical(_flow))
			head += tile.high;

		if (pTile == _listTiles[i])
		{
			box = tile;
			break;
		}
	}
	if (horizontal(_flow))
		box.x = tail;
	else if (vertical(_flow))
		box.y = tail;
}

// Tile is the bounding rectangle in scroll space that we want to show.
// It may be occluded due to scrolling. Adjust scrolling as needed.
// A flow must be marked for scrolling for scroll adjustments to occur at that layer.
void Flow::balance(rect_t tile)
{
	rect_t rect = {0};
	rect_t flow = {0};

	getScrollBox(flow);
	getRect(rect);
	if (_scroll && horizontal(_flow) )
	{
		// room to scroll?
		if (rect.wide >= tile.wide)
		{
			if (tile.x < flow.x)
				flow.x = tile.x;
			else if ( (tile.x + tile.wide) > (flow.x + rect.wide) )
				flow.x = tile.x + tile.wide - rect.wide;
			setScrollPoint(flow.getPoint());
		}
	}
	else if (_scroll && vertical(_flow) )
	{
		// room to scroll?
		if (rect.high >= tile.high)
		{
			if (tile.y < flow.y)
				flow.y = tile.y;
			else if ( (tile.y + tile.high) > (flow.y + rect.high) )
				flow.y = tile.y + tile.high - rect.high;
			setScrollPoint(flow.getPoint());
		}
	}
	else if ( !isRoot() )
	{
		Flow *parent = getContainer();
		rect_t box = {0};
		parent->getTileBox(this, box);
		if (Flow::horizontal(parent->getFlow()) )
		{
			tile.x += box.x;
			tile.y = box.y;
		}
		else if (Flow::vertical(parent->getFlow()) )
		{
			tile.y += box.y;
			tile.x = box.x;
		}
		parent->balance(tile);
	}
}

void Flow::setBorder(meter_t border)
{
	_thick.local = true;
	_thick.thick = border;
}

// rect is visible area
// box is scrollable area
// border is excluded
// TODO: merge this code with scroll(x,y) to 
// take scroll offset into account in layout.
void Flow::layout(rect_t rect, rect_t box)
{
	meter_t space = _tile.getTheme().getThick(_space);
	size_t size = _listTiles.size();
	rect_t work = rect;
	rect_t play = {0};
	meter_t weight = 0;
	meter_t min = 0;

	getInsideMin(_flow, min);
	for (size_t i = 0; i < size; i++)
	{
		meter_t tile = 0;
		ITile* pTile = _listTiles[i];
		pTile->getWeight(_flow, tile);
		_ASSERT(tile >= 0);
		weight += tile;
	}

	if ( horizontal(_flow) )
	{
		meter_t slack = rect.wide - min;
		if (slack < 0) slack = 0;
		meter_t step = 0;
		for (size_t i = 0; i < size; i++)
		{
			ITile* pTile = _listTiles[i];
			FlowDesc desc = {0};
			pTile->getMin(_flow, desc._min);
			pTile->getMax(_flow, desc._max);
			pTile->getWeight(_flow, desc._weight);

			play.x = 0;
			play.y = box.y;
			play.high = box.high;
			play.wide = desc._min;
			if (weight > 0)
			{
				meter_t max = desc._max - desc._min;
				if (max < 0) max = 0;
				//_ASSERT(max >= 0);
				meter_t grow = (slack * desc._weight) / weight;
				if (grow > max)
					grow = max;
				play.wide += grow;
				slack -= grow;
			}
			weight -= desc._weight;
			work.wide = play.wide;
			// clipped left?
			if ( (step + play.wide) < box.x )
			{
				play.x = play.wide;
				work.wide = 0;
			}
			// overlapped left?
			else if (step < box.x)
			{
				play.x = box.x - step;
				work.wide = play.wide - play.x;
			}
			else
			{
				play.x = 0;
			}
			// clipped right?
			if (step >= (box.x + rect.wide) )
			{
				work.wide = 0;
			}
			// overlap right?
			else if ( (step + play.wide) > (box.x + rect.wide) )
			{
				meter_t span = box.x + rect.wide - step;
				if (work.wide < play.wide)
					span -= play.wide - work.wide;
				work.wide = span;
			}
			meter_t right = rect.right();
			if ( (work.wide + work.x) > right )
				work.wide = right - work.x;
			pTile->setScrollBox(play);
			pTile->setRect(work);
			step += play.wide;
			step += space;
			work.x += work.wide;
			work.x += space;
			if (work.x > right)
				work.x = right;
		}
	}
	else if ( vertical(_flow) )
	{
		work.high = 0;

		meter_t slack = rect.high - min;
		if (slack < 0) slack = 0;
		meter_t step = 0;
		for (size_t i = 0; i < size; i++)
		{
			ITile* pTile = _listTiles[i];
			FlowDesc desc = {0};
			pTile->getMin(_flow, desc._min);
			pTile->getMax(_flow, desc._max);
			pTile->getWeight(_flow, desc._weight);

			play.y = 0;
			play.x = box.x;
			play.wide = box.wide;
			play.high = desc._min;
			if (weight > 0)
			{
				meter_t max = desc._max - desc._min;
				if (max < 0) max = 0;
				//_ASSERT(max >= 0);
				meter_t grow = (slack * desc._weight) / weight;
				if (grow > max)
					grow = max;
				play.high += grow;
				slack -= grow;
			}
			weight -= desc._weight;
			work.high = play.high;
			// clipped left?
			if ( (step + play.high) < box.y )
			{
				play.y = play.high;
				work.high = 0;
			}
			// overlapped left?
			else if (step < box.y)
			{
				play.y = box.y - step;
				work.high = play.high - play.y;
			}
			else
			{
				play.y = 0;
			}
			// clipped right?
			if (step >= (box.y + rect.high) )
			{
				work.high = 0;
			}
			// overlap right?
			else if ( (step + play.high) > (box.y + rect.high) )
			{
				meter_t span = box.y + rect.high - step;
				if (work.high < play.high)
					span -= play.high - work.high;
				work.high = span;
			}
			meter_t bottom = rect.bottom();
			if ( (work.high + work.y) > bottom)
				work.high = bottom - work.y;
			pTile->setScrollBox(play);
			pTile->setRect(work);
			work.y += work.high;
			work.y += space;
			step += play.high;
			step += space;
			if (work.y > bottom)
				work.y = bottom;
		}
	}
	setChanged(true);
}

const Theme::Font &Flow::getFont() const
{
	return _tile.getFont();
}

void Flow::setFont(const Theme::Font &font)
{
	_tile.setFont(font);
}
