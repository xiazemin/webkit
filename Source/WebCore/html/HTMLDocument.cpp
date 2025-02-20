/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Portions are Copyright (C) 2002 Netscape Communications Corporation.
 * Other contributors: David Baron <dbaron@fas.harvard.edu>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Alternatively, the document type parsing portions of this file may be used
 * under the terms of either the Mozilla Public License Version 1.1, found at
 * http://www.mozilla.org/MPL/ (the "MPL") or the GNU General Public
 * License Version 2.0, found at http://www.fsf.org/copyleft/gpl.html
 * (the "GPL"), in which case the provisions of the MPL or the GPL are
 * applicable instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of one of those two
 * licenses (the MPL or the GPL) and not to allow others to use your
 * version of this file under the LGPL, indicate your decision by
 * deleting the provisions above and replace them with the notice and
 * other provisions required by the MPL or the GPL, as the case may be.
 * If you do not delete the provisions above, a recipient may use your
 * version of this file under any of the LGPL, the MPL or the GPL.
 */

#include "config.h"
#include "HTMLDocument.h"

#include "CSSPropertyNames.h"
#include "CommonVM.h"
#include "CookieJar.h"
#include "DocumentLoader.h"
#include "DocumentType.h"
#include "ElementChildIterator.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "HTMLBodyElement.h"
#include "HTMLCollection.h"
#include "HTMLDocumentParser.h"
#include "HTMLElementFactory.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLFrameSetElement.h"
#include "HTMLHtmlElement.h"
#include "HTMLIFrameElement.h"
#include "HTMLNames.h"
#include "HashTools.h"
#include "ScriptController.h"
#include "StyleResolver.h"
#include <wtf/text/CString.h>

namespace WebCore {

using namespace HTMLNames;

HTMLDocument::HTMLDocument(Frame* frame, const URL& url, DocumentClassFlags documentClasses, unsigned constructionFlags)
    : Document(frame, url, documentClasses | HTMLDocumentClass, constructionFlags)
{
    clearXMLVersion();
}

HTMLDocument::~HTMLDocument()
{
}

int HTMLDocument::width()
{
    updateLayoutIgnorePendingStylesheets();
    FrameView* frameView = view();
    return frameView ? frameView->contentsWidth() : 0;
}

int HTMLDocument::height()
{
    updateLayoutIgnorePendingStylesheets();
    FrameView* frameView = view();
    return frameView ? frameView->contentsHeight() : 0;
}

Ref<DocumentParser> HTMLDocument::createParser()
{
    return HTMLDocumentParser::create(*this);
}

static void addLocalNameToSet(HashSet<AtomicStringImpl*>* set, const QualifiedName& qName)
{
    set->add(qName.localName().impl());
}

static HashSet<AtomicStringImpl*>* createHtmlCaseInsensitiveAttributesSet()
{
    // This is the list of attributes in HTML 4.01 with values marked as "[CI]" or case-insensitive
    // Mozilla treats all other values as case-sensitive, thus so do we.
    HashSet<AtomicStringImpl*>* attrSet = new HashSet<AtomicStringImpl*>;

    addLocalNameToSet(attrSet, accept_charsetAttr);
    addLocalNameToSet(attrSet, acceptAttr);
    addLocalNameToSet(attrSet, alignAttr);
    addLocalNameToSet(attrSet, alinkAttr);
    addLocalNameToSet(attrSet, axisAttr);
    addLocalNameToSet(attrSet, bgcolorAttr);
    addLocalNameToSet(attrSet, charsetAttr);
    addLocalNameToSet(attrSet, checkedAttr);
    addLocalNameToSet(attrSet, clearAttr);
    addLocalNameToSet(attrSet, codetypeAttr);
    addLocalNameToSet(attrSet, colorAttr);
    addLocalNameToSet(attrSet, compactAttr);
    addLocalNameToSet(attrSet, declareAttr);
    addLocalNameToSet(attrSet, deferAttr);
    addLocalNameToSet(attrSet, dirAttr);
    addLocalNameToSet(attrSet, disabledAttr);
    addLocalNameToSet(attrSet, enctypeAttr);
    addLocalNameToSet(attrSet, faceAttr);
    addLocalNameToSet(attrSet, frameAttr);
    addLocalNameToSet(attrSet, hreflangAttr);
    addLocalNameToSet(attrSet, http_equivAttr);
    addLocalNameToSet(attrSet, langAttr);
    addLocalNameToSet(attrSet, languageAttr);
    addLocalNameToSet(attrSet, linkAttr);
    addLocalNameToSet(attrSet, mediaAttr);
    addLocalNameToSet(attrSet, methodAttr);
    addLocalNameToSet(attrSet, multipleAttr);
    addLocalNameToSet(attrSet, nohrefAttr);
    addLocalNameToSet(attrSet, noresizeAttr);
    addLocalNameToSet(attrSet, noshadeAttr);
    addLocalNameToSet(attrSet, nowrapAttr);
    addLocalNameToSet(attrSet, readonlyAttr);
    addLocalNameToSet(attrSet, relAttr);
    addLocalNameToSet(attrSet, revAttr);
    addLocalNameToSet(attrSet, rulesAttr);
    addLocalNameToSet(attrSet, scopeAttr);
    addLocalNameToSet(attrSet, scrollingAttr);
    addLocalNameToSet(attrSet, selectedAttr);
    addLocalNameToSet(attrSet, shapeAttr);
    addLocalNameToSet(attrSet, targetAttr);
    addLocalNameToSet(attrSet, textAttr);
    addLocalNameToSet(attrSet, typeAttr);
    addLocalNameToSet(attrSet, valignAttr);
    addLocalNameToSet(attrSet, valuetypeAttr);
    addLocalNameToSet(attrSet, vlinkAttr);

    return attrSet;
}

// https://html.spec.whatwg.org/multipage/dom.html#dom-document-nameditem
std::optional<Variant<RefPtr<DOMWindow>, RefPtr<Element>, RefPtr<HTMLCollection>>> HTMLDocument::namedItem(const AtomicString& name)
{
    if (name.isNull() || !hasDocumentNamedItem(*name.impl()))
        return std::nullopt;

    if (UNLIKELY(documentNamedItemContainsMultipleElements(*name.impl()))) {
        auto collection = documentNamedItems(name);
        ASSERT(collection->length() > 1);
        return Variant<RefPtr<DOMWindow>, RefPtr<Element>, RefPtr<HTMLCollection>> { RefPtr<HTMLCollection> { WTFMove(collection) } };
    }

    auto& element = *documentNamedItem(*name.impl());
    if (UNLIKELY(is<HTMLIFrameElement>(element))) {
        if (auto* domWindow = downcast<HTMLIFrameElement>(element).contentWindow())
            return Variant<RefPtr<DOMWindow>, RefPtr<Element>, RefPtr<HTMLCollection>> { RefPtr<DOMWindow> { domWindow } };
    }

    return Variant<RefPtr<DOMWindow>, RefPtr<Element>, RefPtr<HTMLCollection>> { RefPtr<Element> { &element } };
}

Vector<AtomicString> HTMLDocument::supportedPropertyNames() const
{
    // https://html.spec.whatwg.org/multipage/dom.html#dom-document-namedItem-which
    //
    // ... The supported property names of a Document object document at any moment consist of the following, in
    // tree order according to the element that contributed them, ignoring later duplicates, and with values from
    // id attributes coming before values from name attributes when the same element contributes both:
    //
    // - the value of the name content attribute for all applet, exposed embed, form, iframe, img, and exposed
    //   object elements that have a non-empty name content attribute and are in a document tree with document
    //   as their root;
    // - the value of the id content attribute for all applet and exposed object elements that have a non-empty
    //   id content attribute and are in a document tree with document as their root; and
    // - the value of the id content attribute for all img elements that have both a non-empty id content attribute
    //   and a non-empty name content attribute, and are in a document tree with document as their root.

    // FIXME: Implement.
    return { };
}

void HTMLDocument::addDocumentNamedItem(const AtomicStringImpl& name, Element& item)
{
    m_documentNamedItem.add(name, item, *this);
    addImpureProperty(AtomicString(const_cast<AtomicStringImpl*>(&name)));
}

void HTMLDocument::removeDocumentNamedItem(const AtomicStringImpl& name, Element& item)
{
    m_documentNamedItem.remove(name, item);
}

void HTMLDocument::addWindowNamedItem(const AtomicStringImpl& name, Element& item)
{
    m_windowNamedItem.add(name, item, *this);
}

void HTMLDocument::removeWindowNamedItem(const AtomicStringImpl& name, Element& item)
{
    m_windowNamedItem.remove(name, item);
}

bool HTMLDocument::isCaseSensitiveAttribute(const QualifiedName& attributeName)
{
    static HashSet<AtomicStringImpl*>* htmlCaseInsensitiveAttributesSet = createHtmlCaseInsensitiveAttributesSet();
    bool isPossibleHTMLAttr = !attributeName.hasPrefix() && (attributeName.namespaceURI() == nullAtom);
    return !isPossibleHTMLAttr || !htmlCaseInsensitiveAttributesSet->contains(attributeName.localName().impl());
}

bool HTMLDocument::isFrameSet() const
{
    if (!documentElement())
        return false;
    return !!childrenOfType<HTMLFrameSetElement>(*documentElement()).first();
}

Ref<Document> HTMLDocument::cloneDocumentWithoutChildren() const
{
    return create(nullptr, url());
}

}
