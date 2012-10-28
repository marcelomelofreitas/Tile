#include "stdafx.h"
#include "Grid.h"
#include "Text.h"
#include "Button.h"
#include "Fill.h"
#include "IWindow.h"
#include "JSON.h"

/*
Copyright � 2011, 2012 Rick Parrish
*/

using namespace Tiles;

struct Row : public Pane
{
	Row(identity_t id, Theme &theme);
	Row(identity_t id, Theme &theme, Theme::Font& desc);
	virtual ~Row() { };
};

Row::Row(identity_t id, Theme &theme) : Pane(id, theme, eRight)
{
}

Row::Row(identity_t id, Theme &theme, Theme::Font& desc) : Pane(id, theme, desc, eRight)
{
}

struct Header : public Pane
{
	Header(identity_t id, Theme &theme);
	Header(identity_t id, Theme &theme, Theme::Font& desc);
	virtual ~Header() { };
};

Header::Header(identity_t id, Theme &theme) : Pane(id, theme, eRight)
{
}

Header::Header(identity_t id, Theme &theme, Theme::Font& desc) : Pane(id, theme, desc, eRight)
{
}

Grid::Grid(identity_t id, Theme &theme) : Pane(id, theme, eDown), 
	_table(NULL), _scrollVert(NULL), _paneVert(NULL), _scrollHorz(NULL)
{
}

// Sets the content for this grid.
void Grid::setTable(ITable *p)
{
	_table = p;
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
		// remove cell tiles before deleting the row 'cuz the cell tiles belong to the table.
		pKill->clear();
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
		_table->setVisible(0, rows);

		// compose a header row.
		Set *set = _table->getHeader();
		Header *row = new Header(0, theme);
		for (size_t col = 0; col < set->Columns.size(); col++)
		{
			// TODO: include a "splitter" control with each Text 
			// tile to allow users to adjust column widths.
			Flow desc = {0};
			// Button is our column heading.
			Button *button = new Button(col, theme, textFont, set->Columns[col]->Name);
			// Cloning the controls Flow causes the header tiles 
			// to align proportionately with the data cells.
			IControl *pControl = set->Columns[col]->Control;
			pControl->getFlow(eRight, desc);
			button->setFlow(eRight, desc);
			// add column heading to header row.
			row->Add(button);
			// column heading click will allow choice of pre-defined sort selection.
			button->Click.bind(this, &Grid::clickHeader);
		}
		// add the header row.
		Add(row);

		for (i = 0; i < rows; i++)
		{
			// compose data rows.
			set = _table->getRow(i);
			Row* row = new Row(0, theme);
			if (set == NULL)
			{
				Fill *pFill = new Fill(0, theme);
				row->Add(pFill, 0, 4096, 0);
			}
			else
			{
				for (size_t col = 0; col < set->Columns.size(); col++)
				{
					IControl *pControl = set->Columns[col]->Control;
					row->Add(pControl);
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
	size_t offset = _table->getOffset();
	size_t tail = offset + getVisibleRowCount();
	if (i >= offset && i < tail)
	{
	}
}

// row "i" changed.
void Grid::onChange(size_t i)
{
	size_t offset = _table->getOffset();
	size_t tail = offset + getVisibleRowCount();
	if (i >= offset && i < tail)
	{
		// refresh row
		_listControls[i - offset + 1]->setChanged(true);
	}
}

// row "i" removed.
void Grid::onRemove(size_t i)
{
	i;
}

// row "i" moved to row "j".
void Grid::onMoved(size_t i, size_t j)
{
	i;
	j;
}

// IControl
// key event sink
bool Grid::dispatch(KeyEvent &action)
{
	if ( Pane::dispatch(action) )
		return true;

	if (action._what == KeyEvent::DOWN)
	{
		size_t offset = _table->getOffset();
		size_t rows = getVisibleRowCount();
		size_t index = getIndex();
		// bottom most row in the grid?
		if ( index + 1 == _listControls.size() &&
			// down arrow key?
			action._code == VK_DOWN)
		{
			// yes: scroll down one row.
			if (offset + rows < _table->size())
			{
				_pDesktop->setFocus(false);
				_table->setVisible(offset + 1, rows);
				_pDesktop->setFocus(true);
				setChanged(true);
			}
			return true;
		}
		// top most row in the grid?
		if ( index == 1 && 
			// yes: up arrow key?
			action._code == VK_UP)
		{
			// yes: scroll up one row.
			if (offset > 0)
			{
				_pDesktop->setFocus(false);
				_table->setVisible(offset - 1, rows);
				_pDesktop->setFocus(true);
				setChanged(true);
			}
			return true;
		}

		if (action._code == VK_NEXT)
		{
			// yes: scroll down one page.
			if (offset + rows < _table->size())
			{
				_pDesktop->setFocus(false);
				_table->setVisible(offset + rows, rows);
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
				setChanged(true);
				_pDesktop->setFocus(true);
			}
			return true;
		}
	}
	return false;
}

// mouse event sink
bool Grid::dispatch(MouseEvent &action)
{
	return Pane::dispatch(action);
}

void Grid::clickHeader(Button *control, bool value)
{
	if (value)
	{
		size_t iColumn = control->identity();
		_table->setColumn(iColumn);
		setChanged(true);
	}
}

// serialize
bool Grid::save(JSON::Writer &writer)
{
	writer.writeStartObject();
	writer.writeNamedValue("type", type());
	_tile.save(writer);
	writer.writeEndObject(true);
	return true;
}

// de-serialize
bool Grid::load(JSON::Reader &reader, Theme &theme, const char *type, IControl *&pControl)
{
	bool bOK = false;
	if ( !strcmp(type, Grid::type()) )
	{
		identity_t id = 0;
		Flow horz, vert;
		do
		{
			bOK = reader.namedValue("id", id) ||
				loadFlow(reader, "Horz", horz) ||
				loadFlow(reader, "Vert", vert);
		}
		while (bOK && reader.comma());

		if (bOK)
		{
			Grid *pGrid = new Grid(id, theme);
			pGrid->setFlow(eDown, vert);
			pGrid->setFlow(eRight, horz);
			// nested content cannot be loaded via JSON. Must come from a Table.
			// Don't have a JSON spec for Tables.
			pControl = pGrid;
		}
	}
	return bOK;
}
