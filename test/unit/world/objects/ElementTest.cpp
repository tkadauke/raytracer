#include <gtest/gtest.h>

#include "world/objects/Element.h"
#include "world/objects/ElementFactory.h"

#include <QJsonObject>
#include <QJsonArray>
#include <QString>

namespace ElementTest {
  // Minimal Element subclass that opts in to having children, so the parent/
  // child suite can exercise add/insert/remove/move semantics that the bare
  // Element rejects via canHaveChild() == false. Registered with
  // ElementFactory so the JSON-roundtrip tests can deserialise a child of
  // type "TestElement".
  class TestElement : public Element {
    Q_OBJECT
    Q_PROPERTY(QString tag READ tag WRITE setTag)

  public:
    explicit TestElement(Element* parent = nullptr) : Element(parent) {}

    inline const QString& tag() const { return m_tag; }
    inline void setTag(const QString& tag) { m_tag = tag; }

    inline bool canHaveChild(Element*) const override { return true; }

  private:
    QString m_tag;
  };

  // Self-registering creator so ElementFactory::create(...) works inside the
  // JSON roundtrip tests. The static-bool-initialiser idiom matches the
  // production world/objects/*.cpp registrations. The id has to match
  // QMetaObject::className(), which Qt namespace-qualifies for inline
  // classes ("ElementTest::TestElement"), so the round-trip type lookup
  // resolves to the same Creator that wrote the JSON in the first place.
  static const bool s_registered =
      ElementFactory::self().registerClass<TestElement>("ElementTest::TestElement");

  TEST(Element, ShouldInitializeWithoutParent) {
    Element element;
    EXPECT_EQ(nullptr, element.parent());
    EXPECT_EQ(0, element.childElements().size());
  }

  TEST(Element, ShouldInitializeWithParent) {
    TestElement parent;
    auto* child = new Element(&parent);
    EXPECT_EQ(&parent, child->parent());
  }

  TEST(Element, ShouldGenerateUniqueIds) {
    Element a;
    Element b;
    EXPECT_NE(a.id(), b.id());
  }

  TEST(Element, ShouldGenerateNonEmptyId) {
    Element element;
    EXPECT_FALSE(element.id().isEmpty());
  }

  TEST(Element, ShouldHaveEmptyNameByDefault) {
    Element element;
    EXPECT_TRUE(element.name().isEmpty());
  }

  TEST(Element, ShouldNotBeGeneratedByDefault) {
    Element element;
    EXPECT_FALSE(element.isGenerated());
  }

  TEST(Element, ShouldSetAndGetId) {
    Element element;
    element.setId("custom-id");
    EXPECT_EQ(QString("custom-id"), element.id());
  }

  TEST(Element, ShouldSetAndGetName) {
    Element element;
    element.setName("custom name");
    EXPECT_EQ(QString("custom name"), element.name());
  }

  TEST(Element, ShouldSetAndGetGenerated) {
    Element element;
    element.setGenerated(true);
    EXPECT_TRUE(element.isGenerated());
  }

  TEST(Element, ShouldReturnNameAsDisplayNameWhenNameIsSet) {
    Element element;
    element.setName("named");
    EXPECT_EQ(QString("named"), element.displayName());
  }

  TEST(Element, ShouldReturnClassNameInBracketsWhenNameIsEmpty) {
    Element element;
    EXPECT_EQ(QString("<Element>"), element.displayName());
  }

  TEST(Element, ShouldRejectChildrenByDefault) {
    Element parent;
    Element child;
    EXPECT_FALSE(parent.canHaveChild(&child));
  }

  TEST(Element, ShouldAddRawPointerChild) {
    TestElement parent;
    auto* child = new Element;
    parent.addChild(child);
    ASSERT_EQ(1, parent.childElements().size());
    EXPECT_EQ(child, parent.childElements().first());
    EXPECT_EQ(&parent, child->parent());
  }

  TEST(Element, ShouldAddUniquePointerChild) {
    TestElement parent;
    auto child = std::make_unique<Element>();
    auto* raw = child.get();
    parent.addChild(std::move(child));
    ASSERT_EQ(1, parent.childElements().size());
    EXPECT_EQ(raw, parent.childElements().first());
    EXPECT_EQ(&parent, raw->parent());
  }

  TEST(Element, ShouldInsertChildAtSpecificIndex) {
    TestElement parent;
    auto* a = new Element;
    auto* b = new Element;
    auto* c = new Element;
    parent.addChild(a);
    parent.addChild(c);
    parent.insertChild(1, b);
    ASSERT_EQ(3, parent.childElements().size());
    EXPECT_EQ(a, parent.childElements()[0]);
    EXPECT_EQ(b, parent.childElements()[1]);
    EXPECT_EQ(c, parent.childElements()[2]);
  }

  TEST(Element, ShouldRemoveChildByIndex) {
    TestElement parent;
    auto* a = new Element;
    auto* b = new Element;
    parent.addChild(a);
    parent.addChild(b);
    parent.removeChild(0);
    ASSERT_EQ(1, parent.childElements().size());
    EXPECT_EQ(b, parent.childElements().first());
    EXPECT_EQ(nullptr, a->parent());
    delete a;
  }

  TEST(Element, ShouldRemoveChildByPointer) {
    TestElement parent;
    auto* a = new Element;
    auto* b = new Element;
    parent.addChild(a);
    parent.addChild(b);
    parent.removeChild(a);
    ASSERT_EQ(1, parent.childElements().size());
    EXPECT_EQ(b, parent.childElements().first());
    delete a;
  }

  TEST(Element, ShouldReturnRowInParent) {
    TestElement parent;
    auto* a = new Element;
    auto* b = new Element;
    auto* c = new Element;
    parent.addChild(a);
    parent.addChild(b);
    parent.addChild(c);
    EXPECT_EQ(0, a->row());
    EXPECT_EQ(1, b->row());
    EXPECT_EQ(2, c->row());
  }

  TEST(Element, ShouldReturnMinusOneRowForOrphan) {
    Element orphan;
    EXPECT_EQ(-1, orphan.row());
  }

  TEST(Element, ShouldReparentChildOnInsert) {
    TestElement origin;
    TestElement destination;
    auto* child = new Element;
    origin.addChild(child);
    ASSERT_EQ(1, origin.childElements().size());

    destination.addChild(child);
    EXPECT_EQ(0, origin.childElements().size());
    EXPECT_EQ(1, destination.childElements().size());
    EXPECT_EQ(&destination, child->parent());
  }

  TEST(Element, ShouldMoveChildForward) {
    TestElement parent;
    auto* a = new Element;
    auto* b = new Element;
    auto* c = new Element;
    parent.addChild(a);
    parent.addChild(b);
    parent.addChild(c);

    // Move element at index 0 (a) to position 2 in the QList::move sense
    // (meaning "insert before old index 2"); QList shifts, so the resulting
    // order is [b, a, c].
    parent.moveChild(0, 2);
    EXPECT_EQ(b, parent.childElements()[0]);
    EXPECT_EQ(a, parent.childElements()[1]);
    EXPECT_EQ(c, parent.childElements()[2]);
  }

  TEST(Element, ShouldMoveChildBackward) {
    TestElement parent;
    auto* a = new Element;
    auto* b = new Element;
    auto* c = new Element;
    parent.addChild(a);
    parent.addChild(b);
    parent.addChild(c);

    // Move c (index 2) to index 0 → expected [c, a, b].
    parent.moveChild(2, 0);
    EXPECT_EQ(c, parent.childElements()[0]);
    EXPECT_EQ(a, parent.childElements()[1]);
    EXPECT_EQ(b, parent.childElements()[2]);
  }

  TEST(Element, ShouldNotMoveWhenSourceAndDestinationAreSame) {
    TestElement parent;
    auto* a = new Element;
    auto* b = new Element;
    parent.addChild(a);
    parent.addChild(b);
    parent.moveChild(0, 0);
    EXPECT_EQ(a, parent.childElements()[0]);
    EXPECT_EQ(b, parent.childElements()[1]);
  }

  TEST(Element, ShouldFindSelfById) {
    Element element;
    element.setId("root");
    EXPECT_EQ(&element, element.findById("root"));
  }

  TEST(Element, ShouldFindChildById) {
    TestElement root;
    root.setId("root");
    auto* child = new Element;
    child->setId("child");
    root.addChild(child);
    EXPECT_EQ(child, root.findById("child"));
  }

  TEST(Element, ShouldFindGrandchildById) {
    TestElement root;
    auto* mid = new TestElement;
    auto* leaf = new Element;
    leaf->setId("leaf");
    root.addChild(mid);
    mid->addChild(leaf);
    EXPECT_EQ(leaf, root.findById("leaf"));
  }

  TEST(Element, ShouldReturnNullForUnknownId) {
    Element element;
    EXPECT_EQ(nullptr, element.findById("missing"));
  }

  TEST(Element, ShouldRoundtripIdAndNameViaJson) {
    Element original;
    original.setId("roundtrip-id");
    original.setName("roundtrip name");

    QJsonObject json;
    original.write(json);

    Element decoded;
    decoded.read(json);

    EXPECT_EQ(QString("roundtrip-id"), decoded.id());
    EXPECT_EQ(QString("roundtrip name"), decoded.name());
  }

  TEST(Element, ShouldWriteTypeNameToJson) {
    TestElement element;
    QJsonObject json;
    element.write(json);
    EXPECT_EQ(QString("ElementTest::TestElement"), json["type"].toString());
  }

  TEST(Element, ShouldRoundtripChildrenViaJson) {
    TestElement original;
    original.setId("parent-id");
    auto* child = new TestElement;
    child->setId("child-id");
    child->setTag("hello");
    original.addChild(child);

    QJsonObject json;
    original.write(json);

    TestElement decoded;
    decoded.read(json);

    EXPECT_EQ(QString("parent-id"), decoded.id());
    ASSERT_EQ(1, decoded.childElements().size());
    auto* decodedChild = qobject_cast<TestElement*>(decoded.childElements().first());
    ASSERT_NE(nullptr, decodedChild);
    EXPECT_EQ(QString("child-id"), decodedChild->id());
    EXPECT_EQ(QString("hello"), decodedChild->tag());
  }

  TEST(Element, ShouldExcludeGeneratedChildrenFromJson) {
    TestElement original;
    auto* persisted = new TestElement;
    persisted->setId("kept");
    auto* generated = new TestElement;
    generated->setId("dropped");
    generated->setGenerated(true);
    original.addChild(persisted);
    original.addChild(generated);

    QJsonObject json;
    original.write(json);

    auto children = json["children"].toArray();
    ASSERT_EQ(1, children.size());
    EXPECT_EQ(QString("kept"), children[0].toObject()["id"].toString());
  }
}

#include "ElementTest.moc"
