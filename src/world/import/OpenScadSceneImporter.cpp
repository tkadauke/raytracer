#include "world/import/OpenScadSceneImporter.h"

#include "core/Color.h"
#include "core/formats/ply/PlyFile.h"
#include "core/formats/stl/StlFile.h"
#include "core/geometry/Mesh.h"
#include "core/math/Angle.h"
#include "render/materials/MatteMaterial.h"
#include "render/primitives/MeshPrimitive.h"
#include "render/textures/ConstantColorTexture.h"
#include "world/import/ImportResult.h"
#include "world/import/OpenScadCompiler.h"
#include "world/import/SceneImporterRegistry.h"
#include "world/objects/Box.h"
#include "world/objects/CompiledPrimitive.h"
#include "world/objects/Cylinder.h"
#include "world/objects/Difference.h"
#include "world/objects/Group.h"
#include "world/objects/Intersection.h"
#include "world/objects/Scene.h"
#include "world/objects/Sphere.h"
#include "world/objects/Transformable.h"
#include "world/objects/Union.h"

#include <QFile>
#include <QFileInfo>

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <memory>
#include <optional>
#include <stdexcept>
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
    String,
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
        case '"':
          tokens.push_back(string(location, diagnostics, filename));
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

    Token string(SourceLocation location, std::vector<world::ImportDiagnostic>& diagnostics,
                 const QString& filename) {
      QString text;
      advance();
      while (!atEnd() && peek() != QChar('"')) {
        QChar ch = advance();
        if (ch == QChar('\\') && !atEnd()) {
          ch = advance();
        }
        text.append(ch);
      }

      if (atEnd()) {
        diagnostics.push_back(world::ImportDiagnostic::error(
          "Unterminated string literal", filename, location.line, location.column));
      } else {
        advance();
      }
      return {TokenKind::String, text, 0.0, location};
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

  class EditableParameterScanner {
  public:
    explicit EditableParameterScanner(QString source)
        : m_source(std::move(source)) {
    }

    world::ImportOptionSchemas scan() {
      const bool hasCustomizerBlock = m_source.contains(QStringLiteral("/*<!!start"));
      bool active = !hasCustomizerBlock;
      QStringList pendingDescription;

      for (const QString& line : m_source.split(QChar('\n'))) {
        if (line.contains(QStringLiteral("/*<!!start"))) {
          active = true;
          pendingDescription.clear();
          continue;
        }
        if (line.contains(QStringLiteral("/*<!!end"))) {
          active = false;
          pendingDescription.clear();
          continue;
        }
        if (!active)
          continue;

        if (const auto section = sectionName(line); !section.isEmpty()) {
          m_group = section;
          pendingDescription.clear();
          continue;
        }

        QString lineComment;
        const QString code = codeBeforeLineComment(line, &lineComment);
        if (auto schema = parameterFor(code, lineComment, pendingDescription)) {
          if (!hasParameter(schema->name))
            m_parameters.push_back(*schema);
          pendingDescription.clear();
          continue;
        }

        const QString commentText = standaloneLineComment(line);
        if (!commentText.isEmpty()) {
          pendingDescription << commentText;
        } else if (!code.trimmed().isEmpty()) {
          pendingDescription.clear();
        }
      }

      return m_parameters;
    }

  private:
    std::optional<world::ImportOptionSchema>
    parameterFor(const QString& code, const QString& lineComment,
                 const QStringList& pendingDescription) const {
      if (m_group.compare(QStringLiteral("Hidden"), Qt::CaseInsensitive) == 0)
        return std::nullopt;

      const QString trimmed = code.trimmed();
      if (trimmed.isEmpty())
        return std::nullopt;

      int index = 0;
      const QString name = identifierAt(trimmed, &index);
      if (name.isEmpty())
        return std::nullopt;

      skipSpaces(trimmed, &index);
      if (index >= trimmed.size() || trimmed[index] != QChar('='))
        return std::nullopt;
      ++index;

      const int semicolon = topLevelSemicolon(trimmed, index);
      if (semicolon < 0)
        return std::nullopt;

      const QString value = trimmed.mid(index, semicolon - index).trimmed();
      world::ImportOptionSchema schema;
      schema.name = name;
      schema.group = m_group;
      schema.description = pendingDescription.join(QChar('\n'));
      if (schema.description.trimmed().isEmpty()) {
        schema.description =
          QString("OpenSCAD definition passed as -D %1=value when rebuilding this source asset.")
            .arg(name);
      }

      if (!parseLiteral(schema, value))
        return std::nullopt;

      applyHint(schema, lineComment);
      return schema;
    }

    bool parseLiteral(world::ImportOptionSchema& schema, const QString& value) const {
      if (value.startsWith(QChar('[')) && value.endsWith(QChar(']'))) {
        schema.type = world::ImportOptionType::String;
        schema.defaultValue = normalizedExpression(value);
        return true;
      }

      if (value.startsWith(QChar('"')) && value.endsWith(QChar('"'))) {
        schema.type = world::ImportOptionType::String;
        schema.defaultValue = normalizedExpression(value);
        return true;
      }

      if (value == QStringLiteral("true") || value == QStringLiteral("false")) {
        schema.type = world::ImportOptionType::Boolean;
        schema.defaultValue = value == QStringLiteral("true");
        return true;
      }

      bool ok = false;
      const double number = value.toDouble(&ok);
      if (ok) {
        schema.type = world::ImportOptionType::Double;
        schema.defaultValue = number;
        return true;
      }

      return false;
    }

    void applyHint(world::ImportOptionSchema& schema, const QString& lineComment) const {
      const QString hint = lineComment.trimmed();
      if (hint.isEmpty())
        return;

      if (hint.startsWith(QChar('[')) && hint.endsWith(QChar(']'))) {
        const QString content = hint.mid(1, hint.size() - 2).trimmed();
        if (content.contains(QChar(','))) {
          schema.choices = choicesFromHint(content, isStringExpression(schema.defaultValue));
          if (!schema.choices.isEmpty()) {
            schema.type = world::ImportOptionType::Choice;
            schema.defaultValue = expressionTextForValue(schema.defaultValue);
          }
          return;
        }

        applyRangeHint(schema, content);
        return;
      }

      bool ok = false;
      const double step = hint.toDouble(&ok);
      if (ok && step > 0.0)
        schema.step = step;
    }

    void applyRangeHint(world::ImportOptionSchema& schema, const QString& content) const {
      const auto parts = content.split(QChar(':'), Qt::SkipEmptyParts);
      if (parts.size() != 2 && parts.size() != 3)
        return;

      bool minOk = false;
      bool maxOk = false;
      bool stepOk = false;
      const double minimum = parts[0].trimmed().toDouble(&minOk);
      const double maximum = parts.last().trimmed().toDouble(&maxOk);
      const double step = parts.size() == 3 ? parts[1].trimmed().toDouble(&stepOk) : 0.0;
      if (!minOk || !maxOk)
        return;

      schema.minimum = minimum;
      schema.maximum = maximum;
      if (parts.size() == 3 && stepOk && step > 0.0)
        schema.step = step;
    }

    QStringList choicesFromHint(const QString& content, bool quoteStringChoices) const {
      QStringList choices;
      for (QString choice : splitTopLevel(content, QChar(','))) {
        choice = choice.trimmed();
        const int labelSeparator = topLevelLabelSeparator(choice);
        if (labelSeparator >= 0)
          choice = choice.left(labelSeparator).trimmed();
        if (choice.isEmpty())
          continue;
        if (quoteStringChoices && !choice.startsWith(QChar('"')))
          choice = quoteOpenScadString(choice);
        choices << normalizedExpression(choice);
      }
      choices.removeDuplicates();
      return choices;
    }

    static QString sectionName(const QString& line) {
      const int open = line.indexOf(QStringLiteral("/*"));
      const int close = line.indexOf(QStringLiteral("*/"), open + 2);
      if (open < 0 || close < 0)
        return {};

      const QString text = line.mid(open + 2, close - open - 2).trimmed();
      if (!text.startsWith(QChar('[')) || !text.endsWith(QChar(']')))
        return {};
      return text.mid(1, text.size() - 2).trimmed();
    }

    static QString codeBeforeLineComment(const QString& line, QString* lineComment) {
      const int comment = lineCommentIndex(line);
      if (comment < 0) {
        if (lineComment)
          *lineComment = {};
        return line;
      }

      if (lineComment)
        *lineComment = line.mid(comment + 2).trimmed();
      return line.left(comment);
    }

    static QString standaloneLineComment(const QString& line) {
      const QString trimmed = line.trimmed();
      if (!trimmed.startsWith(QStringLiteral("//")))
        return {};
      return trimmed.mid(2).trimmed();
    }

    static int lineCommentIndex(const QString& line) {
      bool inString = false;
      bool escaped = false;
      for (int i = 0; i + 1 < line.size(); ++i) {
        const QChar ch = line[i];
        if (inString) {
          if (escaped) {
            escaped = false;
          } else if (ch == QChar('\\')) {
            escaped = true;
          } else if (ch == QChar('"')) {
            inString = false;
          }
          continue;
        }
        if (ch == QChar('"')) {
          inString = true;
        } else if (ch == QChar('/') && line[i + 1] == QChar('/')) {
          return i;
        }
      }
      return -1;
    }

    static QString identifierAt(const QString& text, int* index) {
      skipSpaces(text, index);
      if (*index >= text.size() || !isIdentifierStart(text[*index]))
        return {};

      QString result;
      while (*index < text.size() && isIdentifierContinue(text[*index])) {
        result.append(text[*index]);
        ++(*index);
      }
      return result;
    }

    static int topLevelSemicolon(const QString& text, int start) {
      bool inString = false;
      bool escaped = false;
      int depth = 0;
      for (int i = start; i < text.size(); ++i) {
        const QChar ch = text[i];
        if (inString) {
          if (escaped) {
            escaped = false;
          } else if (ch == QChar('\\')) {
            escaped = true;
          } else if (ch == QChar('"')) {
            inString = false;
          }
          continue;
        }
        if (ch == QChar('"')) {
          inString = true;
        } else if (ch == QChar('[') || ch == QChar('(')) {
          ++depth;
        } else if (ch == QChar(']') || ch == QChar(')')) {
          if (depth > 0)
            --depth;
        } else if (ch == QChar(';') && depth == 0) {
          return i;
        }
      }
      return -1;
    }

    static int topLevelLabelSeparator(const QString& text) {
      bool inString = false;
      for (int i = 0; i < text.size(); ++i) {
        const QChar ch = text[i];
        if (ch == QChar('"')) {
          inString = !inString;
        } else if (ch == QChar(':') && !inString) {
          return i;
        }
      }
      return -1;
    }

    static QStringList splitTopLevel(const QString& text, QChar delimiter) {
      QStringList result;
      bool inString = false;
      bool escaped = false;
      int start = 0;
      for (int i = 0; i < text.size(); ++i) {
        const QChar ch = text[i];
        if (inString) {
          if (escaped) {
            escaped = false;
          } else if (ch == QChar('\\')) {
            escaped = true;
          } else if (ch == QChar('"')) {
            inString = false;
          }
          continue;
        }
        if (ch == QChar('"')) {
          inString = true;
        } else if (ch == delimiter) {
          result << text.mid(start, i - start);
          start = i + 1;
        }
      }
      result << text.mid(start);
      return result;
    }

    static bool isStringExpression(const QVariant& value) {
      const QString text = value.toString().trimmed();
      return text.startsWith('"') && text.endsWith('"');
    }

    static QString normalizedExpression(QString text) {
      return text.trimmed();
    }

    static QString expressionTextForValue(const QVariant& value) {
      if (value.typeName() == QStringLiteral("bool"))
        return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
      if (QString(value.typeName()) == QStringLiteral("QString"))
        return value.toString().trimmed();
      bool ok = false;
      const double number = value.toDouble(&ok);
      if (ok)
        return QString::number(number, 'g', 15);
      return value.toString().trimmed();
    }

    static QString quoteOpenScadString(QString text) {
      text.replace(QChar('\\'), QStringLiteral("\\\\"));
      text.replace(QChar('"'), QStringLiteral("\\\""));
      return QStringLiteral("\"%1\"").arg(text);
    }

    static void skipSpaces(const QString& text, int* index) {
      while (*index < text.size() && text[*index].isSpace())
        ++(*index);
    }

    static bool isIdentifierStart(QChar ch) {
      return ch.isLetter() || ch == QChar('_') || ch == QChar('$');
    }

    static bool isIdentifierContinue(QChar ch) {
      return isIdentifierStart(ch) || ch.isDigit();
    }

    bool hasParameter(const QString& name) const {
      return std::any_of(m_parameters.begin(), m_parameters.end(),
                         [&](const auto& parameter) { return parameter.name == name; });
    }

    QString m_source;
    QString m_group{QStringLiteral("OpenSCAD Parameters")};
    world::ImportOptionSchemas m_parameters;
  };

  world::ImportOptionSchemas scanEditableOpenScadParameters(const QString& filename) {
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
      return {};

    return EditableParameterScanner(QString::fromUtf8(file.readAll())).scan();
  }

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

  bool hasCompileOptions(const world::ImportOptions& options) {
    return options.contains("executable") || options.contains("cacheDirectory") ||
           options.contains("define") || options.contains("outputFormat");
  }
}

namespace world {
  namespace {
    QString meshFormatFor(const QString& outputPath, const ImportOptions& options) {
      const QString explicitFormat = options.value("outputFormat").toString().trimmed().toLower();
      if (!explicitFormat.isEmpty())
        return explicitFormat;
      return QFileInfo(outputPath).suffix().toLower();
    }

    Mesh readMeshFile(const QString& outputPath, const QString& format) {
      Mesh mesh;
      std::ifstream input(outputPath.toStdString(), std::ios::binary);
      if (!input) {
        throw std::runtime_error("unable to open mesh file");
      }

      if (format == QStringLiteral("stl")) {
        StlFile file(input, mesh);
        return mesh;
      }
      if (format == QStringLiteral("ply")) {
        PlyFile file(input, mesh);
        return mesh;
      }

      throw std::invalid_argument(
        QString("unsupported generated mesh format '%1'").arg(format).toStdString());
    }

    std::shared_ptr<render::Material> defaultOpenScadMaterial() {
      auto material = std::make_shared<render::MatteMaterial>(
        std::make_shared<render::ConstantColorTexture>(Colord(0.72, 0.72, 0.68)));
      material->setAmbientCoefficient(0.85);
      material->setDiffuseCoefficient(0.65);
      return material;
    }

    ImportProvenance openScadProvenance(const ImportSourceMetadata& source, const QString& sourceId,
                                        const QString& generatedOutputFormat) {
      ImportProvenance provenance = ImportProvenance::fromSource(source);
      provenance.sourceId = sourceId;
      provenance.category = {
        {"kind", "generated-mesh"},
        {"sourceFormat", source.formatName},
        {"generatedOutputFormat", generatedOutputFormat},
      };
      return provenance;
    }
  }

  QString OpenScadSceneImporter::name() const {
    return "openscad";
  }

  QStringList OpenScadSceneImporter::supportedExtensions() const {
    return {"scad"};
  }

  ImportOptionSchemas OpenScadSceneImporter::optionSchema() const {
    return {
      {"executable",
       ImportOptionType::FilePath,
       "OpenSCAD executable",
       "Path to the openscad executable. When empty, PATH is searched.",
       "",
       false,
       {}},
      {"cacheDirectory",
       ImportOptionType::DirectoryPath,
       "Generated mesh cache",
       "Directory for generated STL meshes keyed by source content and import options.",
       "",
       false,
       {}},
      {"define",
       ImportOptionType::String,
       "OpenSCAD definitions",
       "JSON object of -D name=value definitions passed to OpenSCAD.",
       "",
       false,
       {}},
      {"outputFormat",
       ImportOptionType::String,
       "Generated mesh format",
       "OpenSCAD mesh output format to compile and import. Supported values are stl and ply.",
       "stl",
       false,
       {"stl", "ply"}},
      {"background_color",
       ImportOptionType::String,
       "Background color",
       "Scene background as a CSS color name or hex color when importing a standalone .scad file.",
       "white",
       false,
       {}},
      {"ambient_color",
       ImportOptionType::String,
       "Ambient color",
       "Scene ambient fill light as a CSS color name or hex color when importing a standalone "
       ".scad "
       "file.",
       "#cccccc",
       false,
       {}},
    };
  }

  ImportOptionSchemas OpenScadSceneImporter::editableSourceParameters(const QString& filename,
                                                                      const ImportOptions&) const {
    return scanEditableOpenScadParameters(filename);
  }

  bool OpenScadSceneImporter::wrapDirectImportInSourceAsset() const {
    return true;
  }

  ImportResult OpenScadSceneImporter::importFile(const QString& filename,
                                                 const ImportOptions& options) const {
    if (!hasCompileOptions(options)) {
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

    ImportSourceMetadata source;
    source.importerName = name();
    source.formatName = "OpenSCAD";
    source.sourcePath = filename;

    OpenScadCompileRequest request;
    request.sourcePath = filename;
    request.executablePath = options.value("executable").toString();
    request.cacheDirectory = options.value("cacheDirectory").toString();
    request.outputFormat = options.value("outputFormat", QStringLiteral("stl")).toString();
    request.options = options.values();

    const OpenScadCompiler compiler;
    OpenScadCompileResult compiled = compiler.compile(request);
    source.properties = {
      {"generatedOutputCacheKey", compiled.cacheKey},
      {"generatedOutputPath", compiled.outputPath},
      {"generatedOutputCacheHit", compiled.cacheHit},
    };

    if (!compiled.succeeded) {
      const bool hasOnlyWarnings =
        !compiled.diagnostics.empty() &&
        std::all_of(compiled.diagnostics.begin(), compiled.diagnostics.end(),
                    [](const ImportDiagnostic& diagnostic) { return diagnostic.isWarning(); });
      if (hasOnlyWarnings) {
        auto group = std::make_unique<Group>();
        group->setName(QFileInfo(filename).baseName());
        ImportResult result(std::move(group), source);
        for (const auto& diagnostic : compiled.diagnostics)
          result.addDiagnostic(diagnostic);
        return result;
      }
      return ImportResult::failed(std::move(compiled.diagnostics), source);
    }

    const QString meshFormat = meshFormatFor(compiled.outputPath, options);
    Mesh mesh;
    try {
      mesh = readMeshFile(compiled.outputPath, meshFormat);
    } catch (const std::exception& error) {
      return ImportResult::failed(
        {ImportDiagnostic::error(
          QString("Unable to parse generated OpenSCAD mesh: %1").arg(error.what()),
          compiled.outputPath)},
        source);
    }

    auto group = std::make_unique<Group>();
    group->setName(QFileInfo(filename).baseName());
    setImportProvenance(*group, openScadProvenance(source, QStringLiteral("root"), meshFormat));

    auto primitive = std::make_shared<render::MeshPrimitive>(
      std::move(mesh), render::MeshPrimitive::NormalMode::Flat);
    primitive->setMaterial(defaultOpenScadMaterial());
    auto compiledPrimitive = std::make_unique<CompiledPrimitive>(primitive);
    compiledPrimitive->setName("OpenSCAD Mesh");
    setImportProvenance(*compiledPrimitive,
                        openScadProvenance(source, QStringLiteral("generated-output"), meshFormat));
    group->addChild(std::move(compiledPrimitive));

    ImportResult result(std::move(group), source);
    if (compiled.cacheHit) {
      result.addDiagnostic(
        ImportDiagnostic::warning("Reused cached OpenSCAD mesh output", compiled.outputPath));
    }
    return result;
  }

  bool OpenScadSceneImporter::configureImportedRoot(Element& importedRoot,
                                                    const ImportOptions& options) const {
    (void)options;
    orientImportedRoot(importedRoot);
    return true;
  }

  bool OpenScadSceneImporter::configureImportedScene(Scene& scene, Element& importedRoot,
                                                     const ImportOptions& options) const {
    const ImportedSceneDefaults defaults = importedSceneDefaults(options);
    defaults.applyTo(scene);
    configureImportedRoot(importedRoot, options);
    (void)defaults.frameCamera(scene);
    return true;
  }

  ImportedSceneDefaults
  OpenScadSceneImporter::importedSceneDefaults(const ImportOptions& options) const {
    ImportedSceneDefaults defaults;
    defaults.setBackgroundColorFromOption(options, "background_color");
    defaults.setAmbientColorFromOption(options, "ambient_color");
    return defaults;
  }

  void OpenScadSceneImporter::orientImportedRoot(Element& importedRoot) const {
    auto* transformable = qobject_cast<Transformable*>(&importedRoot);
    if (!transformable)
      return;

    const double halfPi = std::acos(-1.0) / 2.0;
    transformable->setRotation(transformable->rotation() + Vector3d(halfPi, 0.0, 0.0));
    transformable->setMetadataValue("coordinateConversion", "openscad_z_up_to_product_view_up");
  }
}

static bool dummy =
  world::SceneImporterRegistry::self().registerClass<world::OpenScadSceneImporter>("openscad");
