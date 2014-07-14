// (C) Copyright Gert-Jan de Vos and Jan Wilmans 2013.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at 
// http://www.boost.org/LICENSE_1_0.txt)

// Repository at: https://github.com/djeedjay/DebugViewPP/

#include "stdafx.h"
#include "Win32Lib/utilities.h"
#include "Resource.h"
#include "FilterPage.h"

namespace fusion {
namespace debugviewpp {

CFilterPageImpl::CFilterPageImpl(const FilterType::type* filterTypes, size_t filterTypeCount, const MatchType::type* matchTypes, size_t matchTypeCount) :
	m_filterTypes(filterTypes), m_filterTypeCount(filterTypeCount),
	m_matchTypes(matchTypes), m_matchTypeCount(matchTypeCount)
{
}

BEGIN_MSG_MAP_TRY(CFilterPageImpl)
	MSG_WM_INITDIALOG(OnInitDialog)
	MSG_WM_DESTROY(OnDestroy)
	MSG_WM_MOUSEMOVE(OnMouseMove)
	MSG_WM_LBUTTONUP(OnLButtonUp)
	NOTIFY_CODE_HANDLER_EX(LVN_BEGINDRAG, OnDrag)
	NOTIFY_CODE_HANDLER_EX(PIN_ADDITEM, OnAddItem)
	NOTIFY_CODE_HANDLER_EX(PIN_CLICK, OnClickItem)
	NOTIFY_CODE_HANDLER_EX(PIN_ITEMCHANGED, OnItemChanged)
	REFLECT_NOTIFICATIONS()
	CHAIN_MSG_MAP(CDialogResize<CFilterPageImpl>)
END_MSG_MAP_CATCH(ExceptionHandler)

void CFilterPageImpl::ExceptionHandler()
{
	MessageBox(WStr(GetExceptionMessage()), LoadString(IDR_APPNAME).c_str(), MB_ICONERROR | MB_OK);
}

void CFilterPageImpl::InsertFilter(int item, const Filter& filter)
{
	auto pFilterProp = PropCreateSimple(L"", WStr(filter.text));
	pFilterProp->SetBkColor(filter.bgColor);
	pFilterProp->SetTextColor(filter.fgColor);

	auto pBkColor = PropCreateColorItem(L"Background Color", filter.bgColor);
	pBkColor->SetEnabled(filter.filterType != FilterType::Exclude);

	auto pTxColor = PropCreateColorItem(L"Text Color", filter.fgColor);
	pTxColor->SetEnabled(filter.filterType != FilterType::Exclude);

	m_grid.InsertItem(item, PropCreateCheckButton(L"", filter.enable));
	m_grid.SetSubItem(item, 1, pFilterProp);
	m_grid.SetSubItem(item, 2, CreateEnumTypeItem(L"", m_matchTypes, m_matchTypeCount, filter.matchType));
	m_grid.SetSubItem(item, 3, CreateEnumTypeItem(L"", m_filterTypes, m_filterTypeCount, filter.filterType));
	m_grid.SetSubItem(item, 4, pBkColor);
	m_grid.SetSubItem(item, 5, pTxColor);
	m_grid.SetSubItem(item, 6, PropCreateReadOnlyItem(L"", L"�"));
	m_grid.SelectItem(item, 1);
}

void CFilterPageImpl::AddFilter(const Filter& filter)
{
	InsertFilter(m_grid.GetItemCount(), filter);
}

BOOL CFilterPageImpl::OnInitDialog(CWindow /*wndFocus*/, LPARAM /*lInitParam*/)
{
	m_grid.SubclassWindow(GetDlgItem(IDC_FILTER_GRID));
	m_grid.InsertColumn(0, L"", LVCFMT_LEFT, 32, 0);
	m_grid.InsertColumn(1, L"Filter", LVCFMT_LEFT, 200, 0);
	m_grid.InsertColumn(2, L"Match", LVCFMT_LEFT, 60, 0);
	m_grid.InsertColumn(3, L"Type", LVCFMT_LEFT, 60, 0);
	m_grid.InsertColumn(4, L"Bg", LVCFMT_LEFT, 24, 0);
	m_grid.InsertColumn(5, L"Fg", LVCFMT_LEFT, 24, 0);
	m_grid.InsertColumn(6, L"", LVCFMT_LEFT, 16, 0);
	m_grid.SetExtendedGridStyle(PGS_EX_SINGLECLICKEDIT | PGS_EX_ADDITEMATEND);

	UpdateGrid();
	DlgResize_Init(false);
	return TRUE;
}

void CFilterPageImpl::OnDestroy()
{
}

LRESULT CFilterPageImpl::OnDrag(NMHDR* phdr)
{
	NMLISTVIEW& lv = *reinterpret_cast<NMLISTVIEW*>(phdr);

	if (lv.iItem < 0 || lv.iItem >= static_cast<int>(m_filters.size()))
		return 0;

	m_dragItem = lv.iItem;
	POINT pt = { 0 };
	m_dragImage = m_grid.CreateDragImage(lv.iItem, &pt);

	RECT rect = { 0 };
	m_grid.GetItemRect(lv.iItem, &rect, LVIR_LABEL);
	m_dragImage.BeginDrag(0, lv.ptAction.x - rect.left, lv.ptAction.y - rect.top);

	m_grid.ClientToScreen(&pt);
	m_dragImage.DragEnter(nullptr, pt);

	SetCapture();
	return 0;
}

void CFilterPageImpl::OnMouseMove(UINT /*nFlags*/, CPoint point)
{
	if (!m_dragImage.IsNull())
	{
		ClientToScreen(&point);
		m_dragImage.DragMove(point);
	}
}

void CFilterPageImpl::OnLButtonUp(UINT /*nFlags*/, CPoint point)
{
	if (!m_dragImage.IsNull())
	{
		m_dragImage.DragLeave(*this);
		m_dragImage.EndDrag();
		ReleaseCapture();
		m_dragImage.Destroy();

		UINT flags = 0;
		int item = std::min<int>(m_grid.HitTest(point, &flags), m_filters.size() - 1);
		if (item >= 0 && item < static_cast<int>(m_filters.size()))
		{
			auto filter = GetFilter(m_dragItem);
			m_grid.DeleteItem(m_dragItem);
			InsertFilter(item, filter);
		}
	}
}

LRESULT CFilterPageImpl::OnAddItem(NMHDR* /*pnmh*/)
{
	Filter filter;
	AddFilter(filter);
	m_filters.push_back(filter);

	return 0;
}

LRESULT CFilterPageImpl::OnClickItem(NMHDR* pnmh)
{
	auto& nmhdr = *reinterpret_cast<NMPROPERTYITEM*>(pnmh);

	int iItem;
	int iSubItem;
	if (m_grid.FindProperty(nmhdr.prop, iItem, iSubItem) && iSubItem == 6)
	{
		m_grid.DeleteItem(iItem);
		m_filters.erase(m_filters.begin() + iItem);
		return TRUE;
	}

	return FALSE;
}

LRESULT CFilterPageImpl::OnItemChanged(NMHDR* pnmh)
{
	auto& nmhdr = *reinterpret_cast<NMPROPERTYITEM*>(pnmh);

	int iItem;
	int iSubItem;
	if (!m_grid.FindProperty(nmhdr.prop, iItem, iSubItem))
		return FALSE;
	
	if (iSubItem == 3)
	{
		auto& bkColor = dynamic_cast<CPropertyColorItem&>(*m_grid.GetProperty(iItem, 4));
		auto& txColor = dynamic_cast<CPropertyColorItem&>(*m_grid.GetProperty(iItem, 5));
		bkColor.SetEnabled(GetFilterType(iItem) != FilterType::Exclude);
		txColor.SetEnabled(GetFilterType(iItem) != FilterType::Exclude);
	}

	if (iSubItem == 4)
	{
		auto& color = dynamic_cast<CPropertyColorItem&>(*nmhdr.prop);
		auto& edit = dynamic_cast<CPropertyEditItem&>(*m_grid.GetProperty(iItem, 1));
		edit.SetBkColor(color.GetColor());
		return TRUE;
	}

	if (iSubItem == 5)
	{
		auto& color = dynamic_cast<CPropertyColorItem&>(*nmhdr.prop);
		auto& edit = dynamic_cast<CPropertyEditItem&>(*m_grid.GetProperty(iItem, 1));
		edit.SetTextColor(color.GetColor());
		return TRUE;
	}

	return 0;
}

bool CFilterPageImpl::GetFilterEnable(int iItem) const
{
	CComVariant val;
	GetGridItem<CPropertyCheckButtonItem>(m_grid, iItem, 0).GetValue(&val);
	return val.boolVal != VARIANT_FALSE;
}

std::wstring CFilterPageImpl::GetFilterText(int iItem) const
{
	return GetGridItemText(m_grid, iItem, 1);
}

MatchType::type CFilterPageImpl::GetMatchType(int iItem) const
{
	CComVariant val;
	GetGridItem<CPropertyListItem>(m_grid, iItem, 2).GetValue(&val);
	return m_matchTypes[val.lVal];
}

FilterType::type CFilterPageImpl::GetFilterType(int iItem) const
{
	CComVariant val;
	GetGridItem<CPropertyListItem>(m_grid, iItem, 3).GetValue(&val);
	return m_filterTypes[val.lVal];
}

COLORREF CFilterPageImpl::GetFilterBgColor(int iItem) const
{
	CComVariant val;
	GetGridItem<CPropertyColorItem>(m_grid, iItem, 4).GetValue(&val);
	return val.lVal;
}

COLORREF CFilterPageImpl::GetFilterFgColor(int iItem) const
{
	CComVariant val;
	GetGridItem<CPropertyColorItem>(m_grid, iItem, 5).GetValue(&val);
	return val.lVal;
}

Filter CFilterPageImpl::GetFilter(int item) const
{
	return Filter(Str(GetFilterText(item)), GetMatchType(item), GetFilterType(item), GetFilterBgColor(item), GetFilterFgColor(item), GetFilterEnable(item));
}

std::vector<Filter> CFilterPageImpl::GetFilters() const
{
	std::vector<Filter> filters;
	int n = m_grid.GetItemCount();
	filters.reserve(n);

	for (int i = 0; i < n; ++i)
		filters.push_back(Filter(Str(GetFilterText(i)), GetMatchType(i), GetFilterType(i), GetFilterBgColor(i), GetFilterFgColor(i), GetFilterEnable(i)));

	return filters;
}

void CFilterPageImpl::SetFilters(const std::vector<Filter>& filters)
{
	m_filters = filters;
	if (IsWindow())
	{
		UpdateGrid();
	}
}

void CFilterPageImpl::UpdateGrid(int focus)
{
	m_grid.DeleteAllItems();
	for (auto it = m_filters.begin(); it != m_filters.end(); ++it)
		AddFilter(*it);
	m_grid.SelectItem(focus);
}

} // namespace debugviewpp 
} // namespace fusion
