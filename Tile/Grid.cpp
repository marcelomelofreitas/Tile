#include "stdafx.h"
#include "Grid.h"
#include "Text.h"
#include "Button.h"
#include "Fill.h"
#include "IWindow.h"
#include "JSON.h"
#include "Check.h"

/*
Copyright � 2011, 2012 Rick Parrish
*/

using namespace Tiles;

// Row container for cells that edit the same row of data.
struct Row : public Pane
{
	Row(identity_t id, Theme &theme);
	Row(identity_t id, Theme &theme, Theme::Font& desc);
	virtual ~Row();
	Set *_set;
};

Row::Row(identity_t id, Theme &theme) : Pane(id, theme, eRight), _set(NULL)
{
}

Row::Row(identity_t id, Theme &theme, Theme::Font& desc) : Pane(id, theme, desc, eRight), _set(NULL)
{
}

Row::~Row()
{
	// focus?
	if ( getFocus() )
	{
		// yes: one of our cells has the focus so clean that up here.
		IControl *null = NULL;
		_pDesktop->setFocus(null);
	}
	// remove cell tiles before deleting the row 'cuz the cell tiles belong to the table.
	clear();
}

// Header - container of column heading buttons for sorting and resizing.
struct Header : public Pane
{
	Header(identity_t id, Theme &theme);
	Header(identity_t id, Theme &theme, Theme::Font& desc);
	//virtual ~Header() { };
	bool getColumn(point_t pt, size_t &col, meter_t &datum) const;
	void dragColumn(size_t col, meter_t x);
	virtual bool contains(point_t pt) const;
	// enable
	virtual bool getEnable() const;
};

Header::Header(identity_t id, Theme &theme) : Pane(id, theme, eRight)
{
}

Header::Header(identity_t id, Theme &theme, Theme::Font& desc) : Pane(id, theme, desc, eRight)
{
}

//
bool Header::getColumn(point_t pt, size_t &col, meter_t &datum) const
{
	size_t size = col = _listControls.size();
	rect_t left, right = {0};
	if ( size > 0 && contains(pt) )
	{
		_listControls[0]->getRect(left);
		for (size_t i = 1; i < size; i++)
		{
			_listControls[i]->getRect(right);
			// clicked on the border between left and right headers?
			if (pt.x >= left.right(true) && pt.x < right.left(true))
			{
				// yep: return column index to the left.
				datum = left.left(true) + pt.x - left.right(true);
				col = i - 1;
				return true;
			}
			left = right;
		}
	}
	return false;
}

// Adjusts column width based on mouse position.
// x is the new width.
// Actually adjusts the min value up or down to effect change.
// This has a rubbery effect as drag distance does not quite line up.
void Header::dragColumn(size_t col, meter_t x)
{
	rect_t rect = {0};
	rect_t head = {0};
	Flow flow = {0};
	IControl *column = _listControls[col];

	getRect(head);
	column->getFlow(eRight, flow);
	column->getRect(rect);
	meter_t move = x - rect.wide;
	flow._min = flow._min + move;
	if (move != 0)
	{
		if (flow._min >= 0 && flow._min <= flow._max)
		{
			column->setFlow(eRight, flow);
			setRect(head);
		}
	}
}

// Normally, the grid line space between child controls isn't included when 
// determining whether a point belongs inside the pane. However, we want to 
// be able to adjust the space between columns by clicking on the grid line
// between two column header buttons so override contains here.
bool Header::contains(point_t pt) const
{
	return _tile.contains(pt);
}

bool Header::getEnable() const
{
	// ignore container's enable state.
	// we always want to click on header buttons even when underlying data isn't editable.
	return _enable;
}

// The grid control. It's a Pane control with some extra logic for rows and column headings.
Grid::Grid(identity_t id, Theme &theme) : Pane(id, theme, eDown), 
	_table(NULL), _scrollVert(NULL), _paneVert(NULL), _scrollHorz(NULL), 
	_capture(false), _drag(0), _datum(0), _multi(false), _cursor(0), 
	_auto(false)
{
}

// Sets the content for this grid using an interface that is oblivious to the underlying data type.
void Grid::setTable(ITable *p, bool multi)
{
	if (p)
	{
		// who knows what state this table was left in last?
		p->setOffset(0);
		p->clearSelect();
	}

	_cursor = 0;
	_table = p;
	_multi = multi;
	if (p)
		p->follow(this);
	reflow();
}

// Called when grid resized or new table assigned.
// This implementation has plenty of room for improvement.
// Currently everything is destroyed and recreated.
// TODO: rewrite to only create / delete rows as needed.
void Grid::reflow()
{
	Theme& theme = _tile.getTheme();
	Theme::Font textFont = {Theme::eText, theme.Text};
	// compute number of visible rows
	size_t rows = getVisibleRowCount();

	// controls are tiles so deleting all tiles also deletes all controls.
	_listControls.clear();
	// delete all tiles.
	size_t i = _listTiles.size();

	// all but first row are data rows.
	while (i > 1)
	{
		// get next data row.
		Row *pKill = reinterpret_cast<Row *>(_listTiles[--i]);
		delete pKill;
	}

	// first row is header row.
	if (i > 0)
	{
		ITile *pKill = _listTiles[--i];
		// delete the header row (all of which is owned by this grid).
		delete pKill;
	}

	_listTiles.clear();

	if (_table)
	{
		size_t offset = _table->getOffset();
		_table->setVisible(offset, rows);

		Theme::Color capFore(Theme::eCaptionFore, theme.CaptionFore);
		Theme::Color capBack(Theme::eCaptionBack, theme.CaptionBack);
		Theme::Color toolOver(Theme::eToolOver, theme.ToolOver);

		// compose a header row.
		Set *set = _table->getHeader();
		Header *header = new Header(0, theme);
		for (size_t col = 0; col < set->Columns.size(); col++)
		{
			// Headings will allow users to adjust column widths.
			Flow desc = {0};
			// Button is our column heading.
			Button* button = new Button(col + 1, theme, textFont, set->Columns[col]->Name);
			button->setTip(set->Columns[col]->Notes);

			button->setColorUp(capFore, capBack);
			button->setColorDn(capFore, capBack);
			button->setColorOver(capFore, toolOver);
			button->setColorFocus(capFore, toolOver);

			// Cloning the controls Flow causes the header tiles 
			// to align proportionately with the data cells.
			IControl *pControl = set->Columns[col]->Control;
			pControl->getFlow(eRight, desc);
			button->setFlow(eRight, desc);
			// add column heading to header row.
			header->Add(button);
			// column heading click will allow choice of pre-defined sort selection.
			button->Click.bind(this, &Grid::clickHeader);
		}
		// add the header row.
		Add(header);

		// for each row
		for (i = 0; i < rows; i++)
		{
			// compose data rows.
			Row* row = new Row(0, theme);
			row->_set = _table->getRow(i);
			if (row->_set == NULL)
			{
				Fill *pFill = new Fill(0, theme);
				row->Add(pFill, 0, 4096, 0);
			}
			else
			{
				// for each column
				for (size_t col = 0; col < row->_set->Columns.size(); col++)
				{
					Flow flow = {0};
					// Get flow from header. adjustments to column widths are applied to 
					// header row. Set the data rows to match so columns align.
					// Appropriate for grid?
					if (_table->getHeader()->Columns[col]->grid)
					{
						IControl *pControl = _table->getHeader()->Columns[col]->Control;
						pControl->getFlow(eRight, flow);
						pControl = row->_set->Columns[col]->Control;
						pControl->setFlow(eRight, flow);
						row->Add(pControl);
					}
				}
			}
			// add data row.
			Add(row);
		}
	}
	else
	{
		// no table so use empty filler.
		Fill *pFill = new Fill(0, theme);
		Add(pFill);
	}
	Pane::reflow();
}

void Grid::setRect(const rect_t &rect)
{
	_tile.setRect(rect);
	reflow();
}

size_t Grid::getVisibleRowCount()
{
	const Theme& theme = _tile.getTheme();
	meter_t height = theme.Text._height;
	rect_t rect = {0};
	_tile.getRect(rect);

	meter_t space = theme.getThick(_space);

	// compute number of visible rows.
	size_t rows = (rect.high + space) / (height + space);

	if (rows > 1)
		rows--;

	return rows;
}

// row "i" added.
void Grid::onAdded(size_t i)
{
	i;
	reflow();
}

// row "i" changed.
void Grid::onChange(size_t i)
{
	i;
	// no need to do anything because the data notifies the property set directly.
}

// row "i" removed.
void Grid::onRemove(size_t i)
{
	i;
	reflow();
}

// row "i" moved to row "j".
void Grid::onMoved(size_t i, size_t j)
{
	i;
	j;
	reflow();
}

// IControl
// key event sink
bool Grid::dispatch(KeyEvent &action)
{
	_cursor = getRowIndex();
	if ( Pane::dispatch(action) )
		return true;

	if (action._what == KeyEvent::DOWN)
	{
		size_t offset = _table->getOffset();
		size_t rows = getVisibleRowCount();
		size_t index = getIndex();
		// down arrow key?
		if (action._code == VK_DOWN)
		{
			// bottom most row in the grid?
			if ( index + 1 == _listControls.size() )
			{
				// yes: scroll down one row.
				if (offset + rows < _table->size())
				{
					_pDesktop->setFocus(false);
					_table->setVisible(offset + 1, rows);
					onRowIndexChanged();
					_pDesktop->setFocus(true);
					setChanged(true);
					// copy heading flow to rows.
					return true;
				}
				if ( getEnable() )
					return true;
			}
			// don't allow scrolling beyond grid when editing is enabled.
			if ( getEnable() && ( (index + 1) < _listControls.size() ) )
			{
				// If next visible row slot does not have a property set, don't advance.
				Row *row = reinterpret_cast<Row *>(_listControls[index + 1]);
				return row->_set == NULL;
			}
		}
		// up arrow key?
		if (action._code == VK_UP)
		{
			// top most row in the grid?
			if ( index == 1)
			{
				// yes: scroll up one row.
				if (offset > 0)
				{
					_pDesktop->setFocus(false);
					_table->setVisible(offset - 1, rows);
					onRowIndexChanged();
					_pDesktop->setFocus(true);
					setChanged(true);
					return true;
				}
				// don't allow scrolling beyond grid when editing is enabled.
				if ( getEnable() )
					return true;
			}
		}

		if (action._code == VK_NEXT)
		{
			// yes: scroll down one page.
			if (offset + rows < _table->size())
			{
				_pDesktop->setFocus(false);
				_table->setVisible(offset + rows, rows);
				onRowIndexChanged();
				setChanged(true);
				_pDesktop->setFocus(true);
			}
			return true;
		}

		if (action._code == VK_PRIOR)
		{
			// yes: scroll down one page.
			if (offset > 0)
			{
				if (offset > rows)
					offset -= rows;
				else
					offset = 0;
				_pDesktop->setFocus(false);
				_table->setVisible(offset, rows);
				onRowIndexChanged();
				setChanged(true);
				_pDesktop->setFocus(true);
			}
			return true;
		}

		if (action._code == VK_ESCAPE)
		{
			setEnable(false);
			return true;
		}

		if (action._code == VK_F2)
		{
			setEnable(true);
			return true;
		}
	}
	return false;
}

// mouse event sink
bool Grid::dispatch(MouseEvent &action)
{
	_cursor = getRowIndex();
	// captured mouse for column header width sizing?
	if (_capture)
	{
		// yes: mouse moved?
		if (action._what == MouseEvent::eMove)
		{
			// yep:
			Header *header = reinterpret_cast<Header *>(_listTiles[0]);
			header->dragColumn(_drag, action._place.x - _datum);
			return true;
		}
		else if ( action._what == MouseEvent::eUpClick && (action._button & MouseEvent::eLeft) )
		{
			Header *header = reinterpret_cast<Header *>(_listTiles[0]);
			_pDesktop->setCapture(NULL);
			_capture = false;

			Set *set = _table->getHeader();
			size_t cols = header->getControlCount();
			size_t size = _listControls.size();
			// each column
			for (size_t col = 0; col < cols; col++)
			{
				Flow flow = {0};
				IControl *control = header->getControl(col);
				control->getFlow(eRight, flow);
				// update the header too.
				set->Columns[col]->Control->setFlow(eRight, flow);
				// each row.
				for (size_t row = 1; row < size; row++)
				{
					Row *gee = reinterpret_cast<Row *>(_listControls[row]);
					control = gee->getControl(col);
					// NULL implies a Fill tile for empty space.
					if (control != NULL)
						control->setFlow(eRight, flow);
				}
			}
			Pane::reflow();
			return true;
		}
		return false;
	}

	if ( Pane::dispatch(action) )
		return true;

	if (action._what == MouseEvent::eDoubleClick && !DoubleClick.empty())
	{
		for (size_t i = 1; i < _listControls.size(); i++)
		{
			Row *row = reinterpret_cast<Row *>( getControl(i) );
			if ( row->contains(action._place) )
			{
				for (size_t j = 0; j < row->getControlCount(); j++)
				{
					IControl *control = row->getControl(j);
					if ( control->contains(action._place) )
					{
						// commit any pending edits before opening another window.
						_pDesktop->apply();
						DoubleClick(this, _table->getOffset() + i - 1, j);
						break;
					}
				}
				break;
			}
		}
		return true;
	}

	// clicked in the header area but not squarely on a header column.
	size_t i = _listTiles.size();
	if ( action._what == MouseEvent::eDownClick )
	{
		if ( (action._button & MouseEvent::eLeft) && i > 0)
		{
			Header *header = reinterpret_cast<Header *>(_listTiles[0]);
			if ( header->getColumn(action._place, _drag, _datum) )
			{
				_pDesktop->setCapture(this, IWindow::SIZEWE);
				_capture = true;
			}
		}
	}
	return false;
}

void Grid::clickHeader(Button *control)
{
	size_t iColumn = control->identity();
	_table->setColumn(iColumn);
	setChanged(true);
}

size_t Grid::getRowIndex() const
{
	size_t index = 0;
	if (_table)
	{
		index = _table->getOffset() + getIndex();
		if (index > 0) index--;
	}
	return index;
}

void Grid::onIndexChanged(size_t)
{
	size_t row = getRowIndex();
	if (row != _cursor)
	{
		onRowIndexChanged();
	}
	_cursor = row;
}

void Grid::onRowIndexChanged()
{
	size_t row = getRowIndex();
	if ( GetKeyState(VK_SHIFT) < 0 )
	{
		_table->setSelect(_cursor, row);
	}
	else if ( GetKeyState(VK_CONTROL) < 0 )
	{
		bool select = _table->getSelect(row);
		_table->setSelect(row, !select);
	}
}

// Clear selection.
void Grid::clearSelect()
{
	_table->clearSelect();
	setChanged(true);
}
