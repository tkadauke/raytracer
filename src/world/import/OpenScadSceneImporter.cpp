#include "world/import/OpenScadSceneImporter.h"

#include "core/math/Angle.h"
#include "world/import/SceneImporterRegistry.h"
#include "world/objects/Box.h"
#include "world/objects/Cylinder.h"
#include "world/objects/Difference.h"
#include "world/objects/Group.h"
#include "world/objects/Intersection.h"
#include "world/objects/Sphere.h"
#include "world/objects/Union.h"

#include <QFile>
#include <QFileInfo>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

namespace {
  enum class TokenKind {
    Identifier,
    Number,
    LeftParen,
    RightParen,
    LeftBrace,
    RightBrace,
    LeftBracket,
    RightBracket,
    Comma,
    Semicolon,
    Equal,
    End,
    Invalid
  };

  struct SourceLocation {
    int line{1};
    int column{1};
  };

  struct Token {
    TokenKind kind{TokenKind::Invalid};
    QString text;
    double number{0.0};
    SourceLocation location;
  };

  struct Value {
    using Vector = std::vector<Value>;
    std::variant<double, bool, Vector> data{0.0};
  };

  struct Argument {
    QString name;
    Value value;
    SourceLocation location;
  };

  class Lexer {
  public:
    explicit Lexer(QString source)
        : m_source(std::move(source)) {
    }

    std::vector<Token> lex(std::vector<world::ImportDiagnostic>& diagnostics,
                           const QString& filename) {
      std::vector<Token> tokens;
      while (!atEnd()) {
        skipWhitespaceAndComments();
        if (atEnd())
          break;

        const SourceLocation location{m_line, m_column};
        const QChar ch = peek();
        switch (ch.unicode()) {
        case '(':
          tokens.push_back(simple(TokenKind::LeftParen, location));
          advance();
          break;
        case ')':
          tokens.push_back(simple(TokenKind::RightParen, location));
          advance();
          break;
        case '{':
          tokens.push_back(simple(TokenKind::LeftBrace, location));
          advance();
          break;
        case '}':
          tokens.push_back(simple(TokenKind::RightBrace, location));
          advance();
          break;
        case '[':
          tokens.push_back(simple(TokenKind::LeftBracket, location));
          advance();
          break;
        case ']':
          tokens.push_back(simple(TokenKind::RightBracket, location));
          advance();
          break;
        case ',':
          tokens.push_back(simple(TokenKind::Comma, location));
          advance();
          break;
        case ';':
          tokens.push_back(simple(TokenKind::Semicolon, location));
          advance();
          break;
        case '=':
          tokens.push_back(simple(TokenKind::Equal, location));
          advance();
          break;
        default:
          if (ch.isDigit() || ch == QChar('-') || ch == QChar('+') || ch == QChar('.')) {
            tokens.push_back(number(location, diagnostics, filename));
          } else if (isIdentifierStart(ch)) {
            tokens.push_back(identifier(location));
          } else {
            diagnostics.push_back(
              world::ImportDiagnostic::error(QString("Unexpected character '%1'").arg(ch), filename,
                                             location.line, location.column));
            tokens.push_back({TokenKind::Invalid, QString(ch), 0.0, location});
            advance();
          }
          break;
        }
      }
      tokens.push_back({TokenKind::End, QString(), 0.0, {m_line, m_column}});
      return tokens;
    }

  private:
    bool atEnd() const {
      return m_index >= m_source.size();
    }

    QChar peek(int offset = 0) const {
      const int index = m_index + offset;
      return index < m_source.size() ? m_source[index] : QChar();
    }

    QChar advance() {
      const QChar ch = m_source[m_index++];
      if (ch == QChar('\n')) {
        ++m_line;
        m_column = 1;
      } else {
        ++m_column;
      }
      return ch;
    }

    void skipWhitespaceAndComments() {
      bool consumed = true;
      while (consumed && !atEnd()) {
        consumed = false;
        while (!atEnd() && peek().isSpace()) {
          advance();
          consumed = true;
        }
        if (!atEnd() && peek() == QChar('/') && peek(1) == QChar('/')) {
          while (!atEnd() && peek() != QChar('\n'))
            advance();
          consumed = true;
        } else if (!atEnd() && peek() == QChar('/') && peek(1) == QChar('*')) {
          advance();
          advance();
          while (!atEnd() && !(peek() == QChar('*') && peek(1) == QChar('/')))
            advance();
          if (!atEnd()) {
            advance();
            advance();
          }
          consumed = true;
        }
      }
    }

    static bool isIdentifierStart(QChar ch) {
      return ch.isLetter() || ch == QChar('_') || ch == QChar('$');
    }

    static bool isIdentifierContinue(QChar ch) {
      return isIdentifierStart(ch) || ch.isDigit();
    }

    Token simple(TokenKind kind, SourceLocation location) const {
      return {kind, QString(), 0.0, location};
    }

    Token identifier(SourceLocation location) {
      QString text;
      while (!atEnd() && isIdentifierContinue(peek()))
        text.append(advance());
      return {TokenKind::Identifier, text, 0.0, location};
    }

    Token number(SourceLocation location, std::vector<world::ImportDiagnostic>& diagnostics,
                 const QString& filename) {
      QString text;
      if (peek() == QChar('-') || peek() == QChar('+'))
        text.append(advance());
      while (!atEnd() && peek().isDigit())
        text.append(advance());
      if (!atEnd() && peek() == QChar('.')) {
        text.append(advance());
        while (!atEnd() && peek().isDigit())
          text.append(advance());
      }
      if (!atEnd() && (peek() == QChar('e') || peek() == QChar('E'))) {
        text.append(advance());
        if (!atEnd() && (peek() == QChar('-') || peek() == QChar('+')))
          text.append(advance());
        while (!atEnd() && peek().isDigit())
          text.append(advance());
      }

      bool ok = false;
      const double value = text.toDouble(&ok);
      if (!ok) {
        diagnostics.push_back(world::ImportDiagnostic::error(
          QString("Invalid number '%1'").arg(text), filename, location.line, location.column));
      }
      return {TokenKind::Number, text, value, location};
    }

    QString m_source;
    int m_index{0};
    int m_line{1};
    int m_column{1};
  };

  class Parser {
  public:
    Parser(std::vector<Token> tokens, QString filename)
        : m_tokens(std::move(tokens)),
          m_filename(std::move(filename)) {
    }

    world::ImportResult parse(std::vector<world::ImportDiagnostic> diagnostics) {
      m_diagnostics = std::move(diagnostics);

      auto root = std::make_unique<Group>();
      root->setName(QFileInfo(m_filename).completeBaseName());
      root->setMetadataValue(GroupMetadata::sourceFormatKey(), QStringLiteral("openscad"));

      while (!check(TokenKind::End)) {
        if (check(TokenKind::RightBrace)) {
          error("Unexpected '}'", current().location);
          advance();
          continue;
        }
        auto element = statement();
        if (element)
          root->addChild(std::move(element));
      }

      world::ImportSourceMetadata source;
      source.importerName = "openscad";
      source.formatName = "OpenSCAD subset";
      source.sourcePath = m_filename;
      source.properties = {{"nativeSubset", true}};

      if (hasErrors() || root->childElements().empty()) {
        if (root->childElements().empty() && !hasErrors()) {
          m_diagnostics.push_back(world::ImportDiagnostic::error(
            "OpenSCAD source did not contain any supported geometry", m_filename));
        }
        return world::ImportResult::failed(std::move(m_diagnostics), source);
      }

      world::ImportResult result(std::move(root), source);
      for (const auto& diagnostic : m_diagnostics)
        result.addDiagnostic(diagnostic);
      return result;
    }

  private:
    std::unique_ptr<Element> statement() {
      if (!check(TokenKind::Identifier)) {
        error("Expected OpenSCAD construct", current().location);
        synchronize();
        return nullptr;
      }

      const Token name = advance();
      if (!consume(TokenKind::LeftParen, QString("Expected '(' after %1").arg(name.text))) {
        synchronize();
        return nullptr;
      }

      const auto args = argumentList();
      if (!consume(TokenKind::RightParen, "Expected ')' after arguments")) {
        synchronize();
        return nullptr;
      }

      auto children = childBlock();
      auto element = compileCall(name, args, std::move(children));
      consume(TokenKind::Semicolon, QString());
      return element;
    }

    std::vector<Argument> argumentList() {
      std::vector<Argument> args;
      if (check(TokenKind::RightParen))
        return args;

      do {
        Argument arg;
        arg.location = current().location;
        if (check(TokenKind::Identifier) && peek().kind == TokenKind::Equal) {
          arg.name = advance().text;
          advance();
        }
        auto value = parseValue();
        if (value) {
          arg.value = *value;
          args.push_back(arg);
        } else {
          synchronizeArgument();
        }
      } while (consume(TokenKind::Comma, QString()));

      return args;
    }

    std::optional<Value> parseValue() {
      if (check(TokenKind::Number)) {
        Value value;
        value.data = advance().number;
        return value;
      }
      if (check(TokenKind::Identifier)) {
        const Token token = advance();
        if (token.text == "true" || token.text == "false") {
          Value value;
          value.data = token.text == "true";
          return value;
        }
        error(QString("Unsupported expression '%1'").arg(token.text), token.location);
        return std::nullopt;
      }
      if (consume(TokenKind::LeftBracket, QString())) {
        Value::Vector values;
        if (!check(TokenKind::RightBracket)) {
          do {
            auto value = parseValue();
            if (value)
              values.push_back(*value);
          } while (consume(TokenKind::Comma, QString()));
        }
        if (!consume(TokenKind::RightBracket, "Expected ']' after vector"))
          return std::nullopt;
        Value result;
        result.data = std::move(values);
        return result;
      }

      error("Expected a number, boolean, or vector", current().location);
      return std::nullopt;
    }

    std::vector<std::unique_ptr<Element>> childBlock() {
      std::vector<std::unique_ptr<Element>> children;
      if (consume(TokenKind::LeftBrace, QString())) {
        while (!check(TokenKind::RightBrace) && !check(TokenKind::End)) {
          auto child = statement();
          if (child)
            children.push_back(std::move(child));
        }
        consume(TokenKind::RightBrace, "Expected '}' after child block");
      } else if (check(TokenKind::Identifier)) {
        auto child = statement();
        if (child)
          children.push_back(std::move(child));
      }
      return children;
    }

    std::unique_ptr<Element> compileCall(const Token& name, const std::vector<Argument>& args,
                                         std::vector<std::unique_ptr<Element>> children) {
      if (name.text == "cube")
        return cube(args, name.location);
      if (name.text == "sphere")
        return sphere(args, name.location);
      if (name.text == "cylinder")
        return cylinder(args, name.location);
      if (name.text == "translate")
        return transformGroup("translate", args, std::move(children), name.location);
      if (name.text == "rotate")
        return transformGroup("rotate", args, std::move(children), name.location);
      if (name.text == "scale")
        return transformGroup("scale", args, std::move(children), name.location);
      if (name.text == "union")
        return csg<Union>("union", args, std::move(children), name.location);
      if (name.text == "difference")
        return csg<Difference>("difference", args, std::move(children), name.location);
      if (name.text == "intersection")
        return csg<Intersection>("intersection", args, std::move(children), name.location);

      error(QString("Unsupported OpenSCAD construct '%1'").arg(name.text), name.location);
      return nullptr;
    }

    std::unique_ptr<Element> cube(const std::vector<Argument>& args, SourceLocation location) {
      rejectChildrenAndUnknownArgs("cube", args, {"size", "center"}, {"$fn", "$fa", "$fs"});
      const auto size = vectorArg(args, "size", 0, Vector3d(1, 1, 1));
      const bool centered = boolArg(args, "center", 1, false);
      auto box = std::make_unique<Box>();
      box->setName("cube");
      box->setSize(size * 0.5);
      if (!centered)
        box->setPosition(size * 0.5);
      setLineProvenance(*box, location);
      return box;
    }

    std::unique_ptr<Element> sphere(const std::vector<Argument>& args, SourceLocation location) {
      rejectChildrenAndUnknownArgs("sphere", args, {"r", "d"}, {"$fn", "$fa", "$fs"});
      auto result = std::make_unique<Sphere>();
      result->setName("sphere");
      const double radius = numberArg(args, "r", 0, 1.0);
      result->setRadius(numberArg(args, "d", -1, radius * 2.0) * 0.5);
      setLineProvenance(*result, location);
      return result;
    }

    std::unique_ptr<Element> cylinder(const std::vector<Argument>& args, SourceLocation location) {
      rejectChildrenAndUnknownArgs("cylinder", args, {"h", "r", "d", "center"},
                                   {"$fn", "$fa", "$fs"});
      auto result = std::make_unique<Cylinder>();
      result->setName("cylinder");
      const double height = numberArg(args, "h", 0, 1.0);
      const double radius = numberArg(args, "r", 1, 1.0);
      result->setHeight(height);
      result->setRadius(numberArg(args, "d", -1, radius * 2.0) * 0.5);
      result->setRotation(Vector3d(Angled::fromDegrees(90).radians(), 0, 0));
      if (!boolArg(args, "center", -1, false))
        result->setPosition(Vector3d(0, 0, height * 0.5));
      setLineProvenance(*result, location);
      return result;
    }

    std::unique_ptr<Element> transformGroup(const QString& op, const std::vector<Argument>& args,
                                            std::vector<std::unique_ptr<Element>> children,
                                            SourceLocation location) {
      auto group = std::make_unique<Group>();
      group->setName(op);
      if (op == "translate") {
        rejectChildrenAndUnknownArgs(op, args, {"v"}, {});
        group->setPosition(vectorArg(args, "v", 0, Vector3d::null));
      } else if (op == "rotate") {
        rejectChildrenAndUnknownArgs(op, args, {"a"}, {});
        const Vector3d degrees = vectorArg(args, "a", 0, Vector3d::null);
        group->setRotation(Vector3d(Angled::fromDegrees(degrees.x()).radians(),
                                    Angled::fromDegrees(degrees.y()).radians(),
                                    Angled::fromDegrees(degrees.z()).radians()));
      } else if (op == "scale") {
        rejectChildrenAndUnknownArgs(op, args, {"v"}, {});
        group->setScale(vectorArg(args, "v", 0, Vector3d::one));
      }
      addChildren(*group, std::move(children));
      setLineProvenance(*group, location);
      return group;
    }

    template<class T>
    std::unique_ptr<Element> csg(const QString& op, const std::vector<Argument>& args,
                                 std::vector<std::unique_ptr<Element>> children,
                                 SourceLocation location) {
      rejectChildrenAndUnknownArgs(op, args, {}, {});
      auto result = std::make_unique<T>();
      result->setName(op);
      addChildren(*result, std::move(children));
      setLineProvenance(*result, location);
      return result;
    }

    void addChildren(Element& parent, std::vector<std::unique_ptr<Element>> children) {
      for (auto& child : children)
        parent.addChild(std::move(child));
    }

    void setLineProvenance(Element& element, SourceLocation location) {
      world::ImportProvenance provenance;
      provenance.sourceFile = m_filename;
      provenance.lineStart = location.line;
      provenance.lineEnd = location.line;
      provenance.category = {{"format", "openscad"}};
      world::setImportProvenance(element, provenance);
    }

    void rejectChildrenAndUnknownArgs(const QString& construct, const std::vector<Argument>& args,
                                      const std::vector<QString>& supported,
                                      const std::vector<QString>& ignored) {
      for (const auto& arg : args) {
        if (arg.name.isEmpty())
          continue;
        if (std::find(supported.begin(), supported.end(), arg.name) != supported.end())
          continue;
        if (std::find(ignored.begin(), ignored.end(), arg.name) != ignored.end()) {
          m_diagnostics.push_back(world::ImportDiagnostic::warning(
            QString("Ignoring OpenSCAD display parameter '%1'").arg(arg.name), m_filename,
            arg.location.line, arg.location.column));
          continue;
        }
        error(QString("Unsupported parameter '%1' for %2").arg(arg.name, construct), arg.location);
      }
    }

    double numberArg(const std::vector<Argument>& args, const QString& name, int positionalIndex,
                     double fallback) {
      if (const auto* arg = findArg(args, name, positionalIndex)) {
        if (const auto* value = std::get_if<double>(&arg->value.data))
          return *value;
        error(QString("Parameter '%1' must be a number").arg(name), arg->location);
      }
      return fallback;
    }

    bool boolArg(const std::vector<Argument>& args, const QString& name, int positionalIndex,
                 bool fallback) {
      if (const auto* arg = findArg(args, name, positionalIndex)) {
        if (const auto* value = std::get_if<bool>(&arg->value.data))
          return *value;
        error(QString("Parameter '%1' must be a boolean").arg(name), arg->location);
      }
      return fallback;
    }

    Vector3d vectorArg(const std::vector<Argument>& args, const QString& name, int positionalIndex,
                       const Vector3d& fallback) {
      if (const auto* arg = findArg(args, name, positionalIndex)) {
        if (const auto* number = std::get_if<double>(&arg->value.data))
          return Vector3d(*number, *number, *number);
        if (const auto* vector = std::get_if<Value::Vector>(&arg->value.data)) {
          if (vector->size() != 3) {
            error(QString("Parameter '%1' vector must contain three numbers").arg(name),
                  arg->location);
            return fallback;
          }
          std::array<double, 3> components{};
          for (std::size_t i = 0; i != 3; ++i) {
            const auto* number = std::get_if<double>(&(*vector)[i].data);
            if (!number) {
              error(QString("Parameter '%1' vector must contain only numbers").arg(name),
                    arg->location);
              return fallback;
            }
            components[i] = *number;
          }
          return Vector3d(components[0], components[1], components[2]);
        }
        error(QString("Parameter '%1' must be a number or vector").arg(name), arg->location);
      }
      return fallback;
    }

    const Argument* findArg(const std::vector<Argument>& args, const QString& name,
                            int positionalIndex) const {
      for (const auto& arg : args) {
        if (arg.name == name)
          return &arg;
      }
      if (positionalIndex < 0)
        return nullptr;
      int current = 0;
      for (const auto& arg : args) {
        if (arg.name.isEmpty()) {
          if (current == positionalIndex)
            return &arg;
          ++current;
        }
      }
      return nullptr;
    }

    bool hasErrors() const {
      return std::any_of(m_diagnostics.begin(), m_diagnostics.end(),
                         [](const auto& diagnostic) { return diagnostic.isError(); });
    }

    bool check(TokenKind kind) const {
      return current().kind == kind;
    }

    const Token& current() const {
      return m_tokens[m_index];
    }

    const Token& peek() const {
      return m_tokens[std::min(m_index + 1, m_tokens.size() - 1)];
    }

    Token advance() {
      if (!check(TokenKind::End))
        ++m_index;
      return m_tokens[m_index - 1];
    }

    bool consume(TokenKind kind, const QString& message) {
      if (check(kind)) {
        advance();
        return true;
      }
      if (!message.isEmpty())
        error(message, current().location);
      return false;
    }

    void error(const QString& message, SourceLocation location) {
      m_diagnostics.push_back(
        world::ImportDiagnostic::error(message, m_filename, location.line, location.column));
    }

    void synchronizeArgument() {
      while (!check(TokenKind::Comma) && !check(TokenKind::RightParen) && !check(TokenKind::End))
        advance();
    }

    void synchronize() {
      while (!check(TokenKind::Semicolon) && !check(TokenKind::RightBrace) &&
             !check(TokenKind::End))
        advance();
      consume(TokenKind::Semicolon, QString());
    }

    std::vector<Token> m_tokens;
    QString m_filename;
    std::size_t m_index{0};
    std::vector<world::ImportDiagnostic> m_diagnostics;
  };
}

namespace world {

  QString OpenScadSceneImporter::name() const {
    return "openscad";
  }

  QStringList OpenScadSceneImporter::supportedExtensions() const {
    return {"scad"};
  }

  ImportOptionSchemas OpenScadSceneImporter::optionSchema() const {
    return {};
  }

  ImportResult OpenScadSceneImporter::importFile(const QString& filename,
                                                 const ImportOptions&) const {
    ImportSourceMetadata source;
    source.importerName = name();
    source.formatName = "OpenSCAD subset";
    source.sourcePath = filename;
    source.properties = {{"nativeSubset", true}};

    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
      return ImportResult::failed(
        {ImportDiagnostic::error("Unable to read import source", filename)}, source);
    }

    std::vector<ImportDiagnostic> diagnostics;
    Lexer lexer(QString::fromUtf8(file.readAll()));
    auto tokens = lexer.lex(diagnostics, filename);
    return Parser(std::move(tokens), filename).parse(std::move(diagnostics));
  }

}

static bool dummy =
  world::SceneImporterRegistry::self().registerClass<world::OpenScadSceneImporter>("openscad");
