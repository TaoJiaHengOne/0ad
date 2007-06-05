#include "precompiled.h"

#include "ActorEditorListCtrl.h"

#include "AnimListEditor.h"
#include "PropListEditor.h"

#include "AtlasObject/AtlasObject.h"
#include "EditableListCtrl/FieldEditCtrl.h"

ActorEditorListCtrl::ActorEditorListCtrl(wxWindow* parent)
	: DraggableListCtrl(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
		wxLC_REPORT | wxLC_HRULES | wxLC_VRULES | wxLC_SINGLE_SEL)
{
	// Set colours for row backgrounds (which vary depending on what columns
	// have some data)
	#define COLOUR(name, c0, c1) \
	m_ListItemAttr_##name[0].SetBackgroundColour(wxColour c0); \
	m_ListItemAttr_##name[1].SetBackgroundColour(wxColour c1)
	
	const int f=0xFF, e=0xEE, c=0xCC;
	COLOUR(Model,   (f,f,e), (f,f,c));
	COLOUR(Texture, (f,e,e), (f,c,c));
	COLOUR(Anim,    (e,f,e), (c,f,c));
	COLOUR(Prop,    (e,e,f), (c,c,f));
	COLOUR(Colour,  (f,e,f), (f,c,f));
	COLOUR(None,    (f,f,f), (f,f,f));

	#undef COLOUR

	AddColumnType(_("Variant"),		90,  "@name",		new FieldEditCtrl_Text());
	AddColumnType(_("Ratio"),		50,  "@frequency",	new FieldEditCtrl_Text());
	AddColumnType(_("Model"),		140, "mesh",		new FieldEditCtrl_File(_T("art/meshes/"), _("Mesh files (*.pmd, *.dae)|*.pmd;*.dae|All files (*.*)|*.*")));
	AddColumnType(_("Texture"),		140, "texture",		new FieldEditCtrl_File(_T("art/textures/skins/"), _("All files (*.*)|*.*"))); // could be dds, or tga, or png, or bmp, etc, so just allow *
	AddColumnType(_("Animations"),	250, "animations",	new FieldEditCtrl_Dialog(&AnimListEditor::Create));
	AddColumnType(_("Props"),		220, "props",		new FieldEditCtrl_Dialog(&PropListEditor::Create));
	AddColumnType(_("Colour"),		80,  "colour",		new FieldEditCtrl_Colour());
}

void ActorEditorListCtrl::DoImport(AtObj& in)
{
	DeleteData();

	for (AtIter group = in["group"]; group.defined(); ++group)
	{
		for (AtIter variant = group["variant"]; variant.defined(); ++variant)
			AddRow(variant);

		AtObj blank;
		AddRow(blank);
	}

	UpdateDisplay();
}

AtObj ActorEditorListCtrl::DoExport()
{
	AtObj out;

	AtObj group;

	for (size_t i = 0; i < m_ListData.size(); ++i)
	{
		if (IsRowBlank((int)i))
		{
			if (group.defined())
				out.add("group", group);
			group = AtObj();
		}
		else
		{
			AtObj variant = AtlasObject::TrimEmptyChildren(m_ListData[i]);
			group.add("variant", variant);
		}
	}

	if (group.defined())
		out.add("group", group);

	return out;
}

wxListItemAttr* ActorEditorListCtrl::OnGetItemAttr(long item) const
{
	if (item >= 0 && item < (int)m_ListData.size())
	{
		AtObj row (m_ListData[item]);

		// Colour-code the row, depending on the first non-empty column,
		// and also alternate between light/dark.

		if (row["mesh"].hasContent())
			return const_cast<wxListItemAttr*>(&m_ListItemAttr_Model[item%2]);
		else if (row["texture"].hasContent())
			return const_cast<wxListItemAttr*>(&m_ListItemAttr_Texture[item%2]);
		else if (row["animations"].hasContent())
			return const_cast<wxListItemAttr*>(&m_ListItemAttr_Anim[item%2]);
		else if (row["props"].hasContent())
			return const_cast<wxListItemAttr*>(&m_ListItemAttr_Prop[item%2]);
		else if (row["colour"].hasContent())
			return const_cast<wxListItemAttr*>(&m_ListItemAttr_Colour[item%2]);
	}

	return const_cast<wxListItemAttr*>(&m_ListItemAttr_None[item%2]);
}
