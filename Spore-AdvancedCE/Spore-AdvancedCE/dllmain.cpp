// dllmain.cpp : Defines the entry point for the DLL application.
#include "stdafx.h"
#include <Spore\Editors\EditorRigblock.h>
#include "AdvancedCEDebug.h"
#include "AdvCE_EditorControls.h"

AdvCE_EditorControls* editorControls;

void Initialize()
{
	AdvancedCEDebug* advCEdebug = new AdvancedCEDebug();
#ifdef _DEBUG //only allow access to this cheat for devs
	CheatManager.AddCheat("AdvCEDebug", advCEdebug);
# endif
	editorControls = new(AdvCE_EditorControls);
}

void ReparentParts(Editors::cEditor* editor, bool force = false) {
	auto it = eastl::find(editor->mEnabledManipulators.begin(), editor->mEnabledManipulators.end(), id("cEditorManipulator_AdvancedCE"));
	if (force || (editor->IsMode(Editors::Mode::BuildMode) && it != editor->mEnabledManipulators.end()))
	{
		// loop thru all parts
		for (EditorRigblockPtr part : Editor.GetEditorModel()->mRigblocks)
		{
			if (!part->mBooleanAttributes[Editors::kEditorRigblockModelCannotBeParentless] && part->mpParent && editorControls->IsRigblockChassis(part)) {
				part->mpParent = nullptr;
			}
			// fix parts that are their own grandparent
			if (part->mpParent && part->mpParent->mpParent && part->mpParent->mpParent == part) {
				part->mpParent->mpParent = nullptr;
				part->mpParent = nullptr;
				continue;
			}

			if (!force && AdvancedCEDebug::PartCanReparent(part.get()))
			{
				if (editorControls->mpPrevParent && editorControls->mpPrevParent != part &&
					editorControls->mpPrevParent != AdvancedCEDebug::GetSymmetricPart(editorControls->mpPrevParent.get()) && AdvancedCEDebug::IsPartParentOf(part.get(), editorControls->mpPrevParent.get())) {

					part->mpParent = editorControls->mpPrevParent;
				}
				else {
					auto closest = AdvancedCEDebug::GetClosestPart(part.get());
					if (closest && part != closest) { part->mpParent = closest; }
				}
				if (part->mBooleanAttributes[Editors::kEditorRigblockModelActsLikeGrasper] || part->mBooleanAttributes[Editors::kEditorRigblockModelActsLikeFoot])
				{
					HintManager.ShowHint(id("advce-corruptlimb"));
				}
			}
		}
		if (force) {
			editor->Undo(true, true);
		}
	}
}

// If part loses its parent, apply the previous parent, or find the next closest part to attach to.
// TODO: symmetry.
member_detour(Editor_EditHistoryDetour, Editors::cEditor, void(bool, Editors::EditorStateEditHistory*)) {
	void detoured(bool arg1, Editors::EditorStateEditHistory* pStateHistory){
		ReparentParts(this);
		original_function(this, arg1, pStateHistory);
	}
};

member_detour(Editor_SetEditorModelDetour, Editors::cEditor, void(EditorModel*)) {
	void detoured(EditorModel* pEditorModel) {
		bool newModel = !this->GetEditorModel() || (pEditorModel && this->GetEditorModel() && pEditorModel->mKey != this->GetEditorModel()->mKey);
		original_function(this, pEditorModel);
		if (newModel)
			ReparentParts(this, true);
	}
};

void Dispose()
{
	editorControls = nullptr;
}

void AttachDetours()
{
	Editor_EditHistoryDetour::attach(GetAddress(Editors::cEditor, CommitEditHistory));
	Editor_SetEditorModelDetour::attach(GetAddress(Editors::cEditor, SetEditorModel));
}


// Generally, you don't need to touch any code here
BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		ModAPI::AddPostInitFunction(Initialize);
		ModAPI::AddDisposeFunction(Dispose);

		PrepareDetours(hModule);
		AttachDetours();
		CommitDetours();
		break;

	case DLL_PROCESS_DETACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
		break;
	}
	return TRUE;
}

