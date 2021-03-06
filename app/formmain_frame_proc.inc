(*
This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at http://mozilla.org/MPL/2.0/.

Copyright (c) Alexey Torgashin
*)
{$ifdef nn}begin end;{$endif}

procedure TfmMain.FrameOnChangeCaption(Sender: TObject);
var
  Gr: TATGroups;
  Pages: TATPages;
  Frame: TEditorFrame;
  NLocalGroup, NGlobalGroup, NTab: integer;
  D: TATTabData;
begin
  Frame:= Sender as TEditorFrame;
  GetFrameLocation(Frame, Gr, Pages, NLocalGroup, NGlobalGroup, NTab);
  D:= Pages.Tabs.GetTabData(NTab);
  if D=nil then Exit;
  D.TabCaption:= Frame.TabCaption;
  D.TabModified:= Frame.Ed1.Modified or Frame.Ed2.Modified;
  Pages.Invalidate;

  UpdateCaption;
end;

procedure TfmMain.FrameOnUpdateStatus(Sender: TObject);
begin
  UpdateStatus;
end;

procedure TfmMain.DoCodetree_StopUpdate;
var
  F: TEditorFrame;
begin
  F:= CurrentFrame;
  if F=nil then exit;
  F.Adapter[F.Editor].StopTreeUpdate;
end;

procedure TfmMain.FrameOnFocus(Sender: TObject);
var
  F: TEditorFrame;
  Ed: TATSynEdit;
  Tree: TTreeView;
begin
  Ed:= Sender as TATSynEdit;
  F:= Ed.Parent as TEditorFrame;

  F.GetTabGroups.PagesCurrent:= F.GetTabPages;

  UpdateCaption;
  UpdateTabsActiveColor(F);
  UpdateMenuPlugins_Shortcuts;

  if FNewClickedEditor<>Ed then
  begin
    FNewClickedEditor:= Ed;

    Tree:= F.CachedTreeView[Ed];
    if {UiOps.TreeCache and} (Tree.Items.Count>0) then
    begin
      DoTreeviewCopy(Tree, CodeTree.Tree);
      UpdateTreeFilter;
      UpdateTreeSelection(F, Ed);
    end
    else
      UpdateTreeContents;
  end;
end;

function TfmMain.GetFrame(AIndex: integer): TEditorFrame;
var
  D: TATTabData;
  NCount: integer;
begin
  Result:= nil;

  //frame can be inside Groups, GroupsF1..GroupsF3
  NCount:= Groups.GetTabTotalCount;
  if AIndex<NCount then
  begin
    D:= Groups.GetTabDataOfTotalIndex(AIndex);
    if Assigned(D) then
      Result:= D.TabObject as TEditorFrame;
    exit;
  end;

  if FloatGroups then
  begin
    Dec(AIndex, NCount);
    NCount:= GroupsF1.GetTabTotalCount;
    if AIndex<NCount then
    begin
      D:= GroupsF1.GetTabDataOfTotalIndex(AIndex);
      if Assigned(D) then
        Result:= D.TabObject as TEditorFrame;
      exit;
    end;

    Dec(AIndex, NCount);
    NCount:= GroupsF2.GetTabTotalCount;
    if AIndex<NCount then
    begin
      D:= GroupsF2.GetTabDataOfTotalIndex(AIndex);
      if Assigned(D) then
        Result:= D.TabObject as TEditorFrame;
      exit;
    end;

    Dec(AIndex, NCount);
    NCount:= GroupsF3.GetTabTotalCount;
    if AIndex<NCount then
    begin
      D:= GroupsF3.GetTabDataOfTotalIndex(AIndex);
      if Assigned(D) then
        Result:= D.TabObject as TEditorFrame;
      exit;
    end;
  end;
end;

function TfmMain.FrameCount: integer;
begin
  Result:= Groups.GetTabTotalCount;
  if FloatGroups then
  begin
    Inc(Result, GroupsF1.GetTabTotalCount);
    Inc(Result, GroupsF2.GetTabTotalCount);
    Inc(Result, GroupsF3.GetTabTotalCount);
  end;
end;

function TfmMain.CurrentGroups: TATGroups;
var
  CurForm: TForm;
begin
  Result:= Groups;

  CurForm:= Screen.ActiveForm;
  if (CurForm<>Self) and FloatGroups then
  begin
    if CurForm=FFormFloatGroups1 then
      Result:= GroupsF1
    else
    if CurForm=FFormFloatGroups2 then
      Result:= GroupsF2
    else
    if CurForm=FFormFloatGroups3 then
      Result:= GroupsF3
  end;
end;

function TfmMain.CurrentFrame: TEditorFrame;
var
  Gr: TATGroups;
  Pages: TATPages;
  D: TATTabData;
begin
  Result:= nil;
  Gr:= CurrentGroups;
  Pages:= Gr.PagesCurrent;

  D:= Pages.Tabs.GetTabData(Pages.Tabs.TabIndex);
  if D=nil then Exit;
  if D.TabObject=nil then Exit;
  Result:= D.TabObject as TEditorFrame;
end;

function TfmMain.CurrentEditor: TATSynEdit;
var
  F: TEditorFrame;
begin
  F:= CurrentFrame;
  if F<>nil then
    Result:= F.Editor
  else
    Result:= nil;
end;

procedure TfmMain.SetFrame(Frame: TEditorFrame);
var
  Gr: TATGroups;
  Pages: TATPages;
  NLocalGroup, NGlobalGroup, NTab: integer;
begin
  GetFrameLocation(Frame, Gr, Pages, NLocalGroup, NGlobalGroup, NTab);
  Gr.SetPagesAndTabIndex(NLocalGroup, NTab);
end;

procedure TfmMain.FrameOnSaveFile(Sender: TObject);
var
  F: TEditorFrame;
  fn: string;
begin
  F:= Sender as TEditorFrame;
  fn:= F.Filename;
  MsgStatus(
    msgStatusSavedFile+' '+
    ExtractFileName_Fixed(fn)+
    ' ('+ExtractFileDir_Fixed(fn)+')');

  if fn=GetAppPath(cFileOptionsUser) then
    //must use timer
    CurrentEditor.DoCommand(cmd_OpsReloadAndApply);
end;


procedure TfmMain.FrameOnInitAdapter(Sender: TObject);
begin
  with Sender as TATAdapterEControl do
  begin
    DynamicHiliteEnabled:= EditorOps.OpLexerDynamicHiliteEnabled;
    DynamicHiliteMaxLines:= EditorOps.OpLexerDynamicHiliteMaxLines;

    OnParseDone:= @FrameParseDone;
    OnLexerChange:= @FrameLexerChange;
  end;
end;

procedure TfmMain.FrameParseDone(Sender: TObject);
const
  cMinTime = 600;
var
  Frame: TEditorFrame;
begin
  LexerProgress.Hide;
  UpdateTreeContents;


  //restoring of folded state from history is here,
  //after parsing done
  Frame:= (Sender as TATAdapterEControl).Owner as TEditorFrame;
  Frame.DoRestoreFolding;

  if (Sender as TATAdapterEControl).LexerParsingElapsed>=cMinTime then
    DoPyEvent(Frame.Editor, cEventOnLexerParsed, []);
end;

function TfmMain.FrameOfPopup: TEditorFrame;
var
  Pages: TATPages;
  NTab: integer;
  D: TATTabData;
begin
  Result:= nil;
  Pages:= GroupsCtx.PopupPages; if Pages=nil then exit;
  NTab:= GroupsCtx.PopupTabIndex; if NTab<0 then exit;
  D:= Pages.Tabs.GetTabData(NTab); if D=nil then exit;
  Result:= D.TabObject as TEditorFrame;
end;


procedure TfmMain.FrameOnEditorClickMoveCaret(Sender: TObject; APrevPnt,
  ANewPnt: TPoint);
var
  F: TEditorFrame;
  Pnt: TPoint;
begin
  F:= Sender as TEditorFrame;
  if F.MacroRecord then
  begin
    Pnt:= ConvertTwoPointsToDiffPoint(APrevPnt, ANewPnt);
    F.MacroString:= F.MacroString+(Format('%d,%d,%d', [cmd_MouseClickNearCaret, Pnt.X, Pnt.Y])+#10);
  end;
end;

procedure TfmMain.FrameOnEditorClickEndSelect(Sender: TObject; APrevPnt,
  ANewPnt: TPoint);
var
  F: TEditorFrame;
  Pnt: TPoint;
begin
  F:= Sender as TEditorFrame;
  if F.MacroRecord then
  begin
    Pnt:= ConvertTwoPointsToDiffPoint(APrevPnt, ANewPnt);
    F.MacroString:= F.MacroString+(Format('%d,%d,%d', [cmd_MouseClickNearCaretAndSelect, Pnt.X, Pnt.Y])+#10);
  end;
end;

