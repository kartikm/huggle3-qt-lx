//This program is free software: you can redistribute it and/or modify
//it under the terms of the GNU General Public License as published by
//the Free Software Foundation, either version 3 of the License, or
//(at your option) any later version.

//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.

#include "wikiedit.h"

WikiEdit::WikiEdit()
{
    this->Bot = false;
    this->User = NULL;
    this->Minor = false;
    this->NewPage = false;
    this->Size = 0;
    this->User = NULL;
    this->Diff = 0;
    this->OldID = 0;
    this->Summary = "";
    this->Status = StatusNone;
    this->CurrentUserWarningLevel = WarningLevelNone;
    this->OwnEdit = false;
    this->EditMadeByHuggle = false;
    this->TrustworthEdit = false;
    this->RollbackToken = "";
    this->PostProcessing = false;
    this->DifferenceQuery = NULL;
    this->ProcessingQuery = NULL;
    this->Whitelisted = false;
    this->ProcessingDiff = false;
    this->ProcessingRevs = false;
    this->DiffText = "";
    this->Priority = 20;
    this->Score = 0;
    this->Previous = NULL;
    this->Next = NULL;
    this->ProcessingByWorkerThread = false;
    this->ProcessedByWorkerThread = false;
}

WikiEdit::WikiEdit(const WikiEdit &edit)
{
    this->User = NULL;
    this->Page = NULL;
    this->Bot = edit.Bot;
    this->Minor = edit.Minor;
    this->NewPage = edit.NewPage;
    if (edit.Page != NULL)
    {
        this->Page = new WikiPage(edit.Page);
    }
    this->Size = edit.Size;
    if (edit.User != NULL)
    {
        this->User = new WikiUser(edit.User);
    }
    this->Diff = edit.Diff;
    this->OldID = edit.OldID;
    this->CurrentUserWarningLevel = edit.CurrentUserWarningLevel;
    this->Summary = edit.Summary;
    this->Status = edit.Status;
    this->RollbackToken = edit.RollbackToken;
    this->OwnEdit = edit.OwnEdit;
    this->EditMadeByHuggle = edit.EditMadeByHuggle;
    this->TrustworthEdit = edit.TrustworthEdit;
    this->PostProcessing = false;
    this->ProcessingQuery = NULL;
    this->Whitelisted = edit.Whitelisted;
    this->DifferenceQuery = NULL;
    this->ProcessingDiff = false;
    this->ProcessingRevs = false;
    this->DiffText = edit.DiffText;
    this->Priority = edit.Priority;
    this->Previous = NULL;
    this->Next = NULL;
    this->Score = edit.Score;
    this->ProcessingByWorkerThread = false;
    this->ProcessedByWorkerThread = false;
}

WikiEdit::WikiEdit(WikiEdit *edit)
{
    this->User = NULL;
    this->Page = NULL;
    this->Bot = edit->Bot;
    this->Minor = edit->Minor;
    this->NewPage = edit->NewPage;
    if (edit->Page != NULL)
    {
        this->Page = new WikiPage(edit->Page);
    }
    this->Size = edit->Size;
    if (edit->User != NULL)
    {
        this->User = new WikiUser(edit->User);
    }
    this->OldID = edit->OldID;
    this->CurrentUserWarningLevel = edit->CurrentUserWarningLevel;
    this->Summary = edit->Summary;
    this->Diff = edit->Diff;
    this->OwnEdit = edit->OwnEdit;
    this->Status = edit->Status;
    this->RollbackToken = edit->RollbackToken;
    this->EditMadeByHuggle = edit->EditMadeByHuggle;
    this->TrustworthEdit = edit->TrustworthEdit;
    this->PostProcessing = false;
    this->ProcessingQuery = NULL;
    this->Whitelisted = edit->Whitelisted;
    this->DifferenceQuery = NULL;
    this->ProcessingDiff = false;
    this->ProcessingRevs = false;
    this->DiffText = edit->DiffText;
    this->Priority = edit->Priority;
    this->Previous = NULL;
    this->Next = NULL;
    this->Score = edit->Score;
    this->ProcessingByWorkerThread = false;
    this->ProcessedByWorkerThread = false;
}

WikiEdit::~WikiEdit()
{
    //delete this->DifferenceQuery;
    delete this->ProcessingQuery;
    delete this->User;
    delete this->Page;
}

bool WikiEdit::FinalizePostProcessing()
{
    if (this->ProcessedByWorkerThread)
    {
        return true;
    }

    if (this->ProcessingByWorkerThread)
    {
        return false;
    }

    if (!this->PostProcessing)
    {
        return true;
    }

    if (this->ProcessingDiff)
    {
        // check if api was processed
        if (!this->DifferenceQuery->Processed())
        {
            return false;
        }

        if (this->DifferenceQuery->Result->Failed)
        {
            // whoa it ended in error, we need to get rid of this edit somehow now
            this->DifferenceQuery->DeleteLater = false;
            this->DifferenceQuery = NULL;
            this->PostProcessing = false;
            return true;
        }

        // parse the diff now
        QDomDocument d;
        d.setContent(this->DifferenceQuery->Result->Data);
        QDomNodeList l = d.elementsByTagName("rev");
        QDomNodeList diff = d.elementsByTagName("diff");
        // get last id
        if (l.count() > 0)
        {
            QDomElement e = l.at(0).toElement();
            if (e.nodeName() == "rev")
            {
                // check if this revision matches our user
                if (e.attributes().contains("user"))
                {
                    if (e.attribute("user") == this->User->Username)
                    {
                        if (e.attributes().contains("rollbacktoken"))
                        {
                            // let's update it from fresh diff
                            this->RollbackToken = e.attribute("rollbacktoken");
                        }
                    }
                }
                if (e.attributes().contains("comment"))
                {
                    this->Summary = e.attribute("comment");
                }
            }
        }
        if (diff.count() > 0)
        {
            QDomElement e = diff.at(0).toElement();
            if (e.nodeName() == "diff")
            {
                this->DiffText = e.text();
            }
        } else
        {
            Core::DebugLog("Failed to obtain diff for " + this->Page->PageName + " the error was: " + DifferenceQuery->Result->Data);
        }
        // we are done processing the diff
        this->ProcessingDiff = false;
    }

    // check if everything was processed and clean up
    if (this->ProcessingDiff)
    {
        return false;
    }

    this->DifferenceQuery->DeleteLater = false;
    this->DifferenceQuery = NULL;
    this->ProcessingByWorkerThread = true;
    ProcessorThread::EditLock.lock();
    ProcessorThread::PendingEdits.append(this);
    ProcessorThread::EditLock.unlock();
    return false;
}

void WikiEdit::ProcessWords()
{
    int xx = 0;
    QString text = this->DiffText.toLower();
    while (xx<Configuration::LocalConfig_ScoreParts.count())
    {
        QString w = Configuration::LocalConfig_ScoreParts.at(xx).word;
        if (text.contains(w))
        {
            this->Score += Configuration::LocalConfig_ScoreParts.at(xx).score;
            ScoreWords.append(w);
        }
        xx++;
    }
    xx = 0;
    while (xx<Configuration::LocalConfig_ScoreWords.count())
    {
        QString w = Configuration::LocalConfig_ScoreWords.at(xx).word;
        if (text.contains(" " + w + " ") || text.contains(" " + w + ".")
                || text.contains(" " + w + ",") || text.contains(" " + w + "!")
                || text.contains(" " + w + "\n") || text.contains("\n" + w + "\n")
                || text.contains("\n" + w + " "))
        {
            this->Score += Configuration::LocalConfig_ScoreWords.at(xx).score;
            ScoreWords.append(w);
        }
        xx++;
    }
}

void WikiEdit::PostProcess()
{
    if (this->PostProcessing)
    {
        return;
    }
    this->PostProcessing = true;
    //this->ProcessingQuery = new ApiQuery();
    //this->ProcessingQuery->SetAction(ActionQuery);
    //this->ProcessingQuery->Parameters = "prop=revisions&rvprop=ids|user&rvlimit=1&rvtoken=rollback&titles=" +
    //        this->Page->PageName;
    //this->ProcessingQuery->Process();
    this->DifferenceQuery = new ApiQuery();
    this->DifferenceQuery->SetAction(ActionQuery);
    this->DifferenceQuery->Parameters = "prop=revisions&rvtoken=rollback&rvdiffto=prev&titles=" +
            this->Page->PageName;
    this->DifferenceQuery->Target = Page->PageName;
    Core::RunningQueries.append(this->DifferenceQuery);
    this->DifferenceQuery->DeleteLater = true;
    this->DifferenceQuery->Process();
    this->ProcessingDiff = true;
    //this->ProcessingRevs = true;
}

bool WikiEdit::IsPostProcessed()
{
    if (this->Status == StatusPostProcessed)
    {
        return true;
    }
    return false;
}

QMutex ProcessorThread::EditLock;
QList<WikiEdit*> ProcessorThread::PendingEdits;

void ProcessorThread::run()
{
    while(true)
    {
        ProcessorThread::EditLock.lock();
        int e=0;
        while (e<ProcessorThread::PendingEdits.count())
        {
            this->Process(PendingEdits.at(e));
            e++;
        }
        PendingEdits.clear();
        ProcessorThread::EditLock.unlock();
        QThread::usleep(200000);
    }
}

void ProcessorThread::Process(WikiEdit *edit)
{
    // score
    if (edit->User->IP)
    {
        edit->Score += Configuration::LocalConfig_IPScore;
    }
    if (edit->Bot)
    {
        edit->Score += Configuration::LocalConfig_BotScore;
    }

    if (edit->Page->IsUserpage())
    {
        edit->Score -= 200;
    }

    if (edit->Page->IsTalk())
    {
        edit->Score -= 2000;
    }

    edit->Score += edit->User->BadnessScore;

    edit->ProcessWords();

    edit->Status = StatusPostProcessed;
    edit->PostProcessing = false;
    edit->ProcessedByWorkerThread = true;
}

