// Credits: TwilitRealm

#pragma once

#include "component.hpp"
#include "ui.hpp"

#include <cstdio>
#include <cstdlib>
#include <typeinfo>

namespace partyboard::ui {

class Document {
public:
    Document(const Rml::String &source);
    virtual ~Document();

    Document(const Document &) = delete;
    Document &operator=(const Document &) = delete;

    virtual void show();
    virtual void hide(bool close);
    virtual void update();
    virtual bool focus();
    virtual bool visible() const;

    void listen(Rml::Element *element, Rml::EventId event, ScopedEventListener::Callback callback, bool capture = false);
    void listen(Rml::EventId event, ScopedEventListener::Callback callback, bool capture = false)
    {
        listen(mDocument, event, std::move(callback), capture);
    }
    void toggle()
    {
        if (visible()) {
            hide(false);
        }
        else {
            show();
        }
    }
    void push(std::unique_ptr<Document> document)
    {
        if (const char *trace = std::getenv("PARTYBOARD_TRACE_UI_STACK"); trace && trace[0] == '1') {
            std::fprintf(stderr, "Native UI stack: push from=%s to=%s\n", typeid(*this).name(), typeid(*document).name());
        }
        push_document(std::move(document));
        hide(false);
    }
    void pop()
    {
        const char *trace = std::getenv("PARTYBOARD_TRACE_UI_STACK");
        if (trace && trace[0] == '1') {
            std::fprintf(stderr, "Native UI stack: pop begin=%s visible=%d pending=%d closed=%d\n", typeid(*this).name(), visible(), pending_close(), closed());
        }
        hide(true);
        show_top_document();
        if (trace && trace[0] == '1') {
            const auto *top = top_document();
            std::fprintf(stderr, "Native UI stack: pop end=%s visible=%d pending=%d closed=%d top=%s\n", typeid(*this).name(), visible(), pending_close(), closed(), top ? typeid(*top).name() : "none");
        }
    }

    bool pending_close() const
    {
        return mPendingClose;
    }
    bool closed() const
    {
        return mClosed;
    }

protected:
    virtual bool handle_nav_command(Rml::Event &event, NavCommand cmd);

    Rml::ElementDocument *mDocument;
    std::vector<std::unique_ptr<ScopedEventListener>> mListeners;
    bool mPendingClose = false;
    bool mClosed = false;
};

} // namespace partyboard::ui
