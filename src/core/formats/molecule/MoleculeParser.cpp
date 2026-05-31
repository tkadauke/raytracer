#include "core/formats/molecule/MoleculeParser.h"

#include <algorithm>
#include <cctype>
#include <istream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <utility>

using namespace std;

namespace molecule {
  namespace {
    struct Token {
      string text;
      int line{1};
    };

    struct AtomSiteKey {
      int modelId{1};
      string chainId;
      int residueSequence{0};
      string insertionCode;
      string residueName;
      string atomName;

      bool operator<(const AtomSiteKey& other) const {
        return tie(modelId, chainId, residueSequence, insertionCode, residueName, atomName) <
               tie(other.modelId, other.chainId, other.residueSequence, other.insertionCode,
                   other.residueName, other.atomName);
      }
    };

    struct MmcifConnection {
      AtomSiteKey first;
      AtomSiteKey second;
      int order{1};
      int line{-1};
    };

    string trim(string value) {
      const auto first =
        find_if_not(value.begin(), value.end(), [](unsigned char c) { return isspace(c); });
      const auto last = find_if_not(value.rbegin(), value.rend(), [](unsigned char c) {
                          return isspace(c);
                        }).base();
      if (first >= last)
        return "";
      return string(first, last);
    }

    string field(const string& line, size_t offset, size_t length) {
      if (offset >= line.size())
        return "";
      return trim(line.substr(offset, min(length, line.size() - offset)));
    }

    bool startsWith(const string& value, const string& prefix) {
      return value.rfind(prefix, 0) == 0;
    }

    string lowercased(string value) {
      transform(value.begin(), value.end(), value.begin(),
                [](unsigned char ch) { return static_cast<char>(tolower(ch)); });
      return value;
    }

    optional<int> parseOptionalInt(const string& value) {
      const auto trimmed = trim(value);
      if (trimmed.empty() || trimmed == "." || trimmed == "?")
        return nullopt;
      size_t consumed = 0;
      const auto parsed = stoi(trimmed, &consumed);
      if (consumed != trimmed.size())
        throw invalid_argument("invalid integer");
      return parsed;
    }

    optional<double> parseOptionalDouble(const string& value) {
      const auto trimmed = trim(value);
      if (trimmed.empty() || trimmed == "." || trimmed == "?")
        return nullopt;
      size_t consumed = 0;
      const auto parsed = stod(trimmed, &consumed);
      if (consumed != trimmed.size())
        throw invalid_argument("invalid double");
      return parsed;
    }

    string normalizeCifValue(const string& value) {
      if (value == "." || value == "?")
        return "";
      return value;
    }

    string inferElementFromAtomName(const string& atomName) {
      const auto name = trim(atomName);
      string element;
      for (const auto ch : name) {
        if (isalpha(static_cast<unsigned char>(ch))) {
          element += static_cast<char>(toupper(static_cast<unsigned char>(ch)));
          if (element.size() == 2)
            break;
        } else if (!element.empty()) {
          break;
        }
      }
      return element;
    }

    string normalizedAltId(const string& value) {
      return normalizeCifValue(value);
    }

    int bondOrderForConnType(const string& type) {
      const auto normalized = lowercased(type);
      if (normalized.find("doub") != string::npos)
        return 2;
      if (normalized.find("trip") != string::npos)
        return 3;
      return 1;
    }

    vector<Token> tokenizeCif(istream& input) {
      vector<Token> tokens;
      string line;
      int lineNumber = 0;

      while (getline(input, line)) {
        ++lineNumber;
        if (!line.empty() && line[0] == ';') {
          const auto startLine = lineNumber;
          string value = line.substr(1);
          while (getline(input, line)) {
            ++lineNumber;
            if (!line.empty() && line[0] == ';')
              break;
            if (!value.empty())
              value += '\n';
            value += line;
          }
          tokens.push_back({value, startLine});
          continue;
        }

        size_t i = 0;
        while (i < line.size()) {
          while (i < line.size() && isspace(static_cast<unsigned char>(line[i])))
            ++i;
          if (i >= line.size() || line[i] == '#')
            break;

          if (line[i] == '\'' || line[i] == '"') {
            const auto quote = line[i++];
            string value;
            while (i < line.size() && line[i] != quote)
              value += line[i++];
            if (i < line.size())
              ++i;
            tokens.push_back({value, lineNumber});
          } else {
            string value;
            while (i < line.size() && !isspace(static_cast<unsigned char>(line[i])) &&
                   line[i] != '#') {
              value += line[i++];
            }
            tokens.push_back({value, lineNumber});
            if (i < line.size() && line[i] == '#')
              break;
          }
        }
      }

      return tokens;
    }

    void addWarning(MoleculeParseResult& result, const string& message, int line) {
      result.addDiagnostic({DiagnosticSeverity::Warning, message, line});
    }

    void addError(MoleculeParseResult& result, const string& message, int line) {
      result.addDiagnostic({DiagnosticSeverity::Error, message, line});
    }

    string valueFor(const map<string, string>& row, const string& primary,
                    const string& fallback = "") {
      const auto found = row.find(primary);
      if (found != row.end())
        return normalizeCifValue(found->second);
      if (!fallback.empty()) {
        const auto fallbackFound = row.find(fallback);
        if (fallbackFound != row.end())
          return normalizeCifValue(fallbackFound->second);
      }
      return "";
    }

    string valueForAny(const map<string, string>& row, const vector<string>& names) {
      for (const auto& name : names) {
        const auto found = row.find(name);
        if (found != row.end())
          return normalizeCifValue(found->second);
      }
      return "";
    }

    AtomSiteKey atomSiteKeyFromRow(const map<string, string>& row) {
      AtomSiteKey key;
      key.modelId = parseOptionalInt(valueFor(row, "_atom_site.pdbx_PDB_model_num")).value_or(1);
      key.atomName = valueFor(row, "_atom_site.auth_atom_id", "_atom_site.label_atom_id");
      key.residueName = valueFor(row, "_atom_site.auth_comp_id", "_atom_site.label_comp_id");
      key.chainId = valueFor(row, "_atom_site.auth_asym_id", "_atom_site.label_asym_id");
      key.residueSequence =
        parseOptionalInt(valueFor(row, "_atom_site.auth_seq_id", "_atom_site.label_seq_id"))
          .value_or(0);
      key.insertionCode = valueFor(row, "_atom_site.pdbx_PDB_ins_code");
      return key;
    }

    AtomSiteKey connectionPartnerKeyFromRow(const map<string, string>& row, const string& prefix) {
      AtomSiteKey key;
      key.modelId =
        parseOptionalInt(valueForAny(row, {"_struct_conn." + prefix + "_label_model_id",
                                           "_struct_conn.pdbx_" + prefix + "_PDB_model_num"}))
          .value_or(1);
      key.atomName = valueForAny(row, {"_struct_conn." + prefix + "_auth_atom_id",
                                       "_struct_conn." + prefix + "_label_atom_id"});
      key.residueName = valueForAny(row, {"_struct_conn." + prefix + "_auth_comp_id",
                                          "_struct_conn." + prefix + "_label_comp_id"});
      key.chainId = valueForAny(row, {"_struct_conn." + prefix + "_auth_asym_id",
                                      "_struct_conn." + prefix + "_label_asym_id"});
      key.residueSequence =
        parseOptionalInt(valueForAny(row, {"_struct_conn." + prefix + "_auth_seq_id",
                                           "_struct_conn." + prefix + "_label_seq_id"}))
          .value_or(0);
      key.insertionCode = valueForAny(row, {"_struct_conn.pdbx_" + prefix + "_PDB_ins_code",
                                            "_struct_conn." + prefix + "_PDB_ins_code"});
      return key;
    }

    vector<int> parsePdbConectSerials(const string& line) {
      vector<int> serials;
      for (size_t offset = 6; offset + 5 <= line.size(); offset += 5) {
        const auto serial = field(line, offset, 5);
        if (!serial.empty())
          serials.push_back(parseOptionalInt(serial).value_or(0));
      }
      return serials;
    }
  }

  bool MoleculeParseResult::hasErrors() const {
    return any_of(m_diagnostics.begin(), m_diagnostics.end(),
                  [](const Diagnostic& diagnostic) { return diagnostic.isError(); });
  }

  bool MoleculeParseResult::hasWarnings() const {
    return any_of(m_diagnostics.begin(), m_diagnostics.end(),
                  [](const Diagnostic& diagnostic) { return diagnostic.isWarning(); });
  }

  void MoleculeParseResult::addDiagnostic(Diagnostic diagnostic) {
    m_diagnostics.push_back(std::move(diagnostic));
  }

  MoleculeParseResult MoleculeParser::parse(istream& input, const string& format) const {
    const auto normalizedFormat = lowercased(format);
    if (normalizedFormat == "pdb")
      return parsePdb(input);
    if (normalizedFormat == "cif" || normalizedFormat == "mmcif")
      return parseMmcif(input);

    MoleculeParseResult result;
    addError(result, "unsupported molecule format: " + format, -1);
    return result;
  }

  MoleculeParseResult MoleculeParser::parsePdb(istream& input) const {
    MoleculeParseResult result;
    string line;
    int lineNumber = 0;
    int currentModel = 1;
    bool sawExplicitModel = false;
    map<int, size_t> atomIndexBySerial;
    vector<pair<int, int>> conectRecords;

    while (getline(input, line)) {
      ++lineNumber;
      const auto record = field(line, 0, 6);

      if (record == "HEADER") {
        result.molecule().metadata().id = field(line, 62, 4);
      } else if (record == "TITLE") {
        const auto titlePart = field(line, 10, 70);
        if (!titlePart.empty()) {
          if (!result.molecule().metadata().title.empty())
            result.molecule().metadata().title += " ";
          result.molecule().metadata().title += titlePart;
        }
      } else if (record == "MODEL") {
        sawExplicitModel = true;
        try {
          currentModel = parseOptionalInt(field(line, 10, 4)).value_or(currentModel + 1);
        } catch (const invalid_argument&) {
          addWarning(result, "MODEL record has an invalid model id", lineNumber);
          ++currentModel;
        }
      } else if (record == "ENDMDL") {
        if (!sawExplicitModel)
          ++currentModel;
      } else if (record == "ATOM" || record == "HETATM") {
        try {
          const auto x = parseOptionalDouble(field(line, 30, 8));
          const auto y = parseOptionalDouble(field(line, 38, 8));
          const auto z = parseOptionalDouble(field(line, 46, 8));
          if (!x || !y || !z) {
            addWarning(result, "ATOM/HETATM record is missing one or more coordinates", lineNumber);
            continue;
          }

          Atom atom;
          atom.hetero = record == "HETATM";
          atom.serialNumber = parseOptionalInt(field(line, 6, 5)).value_or(0);
          atom.name = field(line, 12, 4);
          atom.alternateLocation = field(line, 16, 1);
          atom.residueName = field(line, 17, 3);
          atom.chainId = field(line, 21, 1);
          atom.residueSequence = parseOptionalInt(field(line, 22, 4)).value_or(0);
          atom.insertionCode = field(line, 26, 1);
          atom.position = Vector3d(*x, *y, *z);
          atom.occupancy = parseOptionalDouble(field(line, 54, 6));
          atom.temperatureFactor = parseOptionalDouble(field(line, 60, 6));
          atom.element = field(line, 76, 2);
          if (atom.element.empty())
            atom.element = inferElementFromAtomName(atom.name);
          atom.modelId = currentModel;
          atom.sourceRecord = record + " " + to_string(atom.serialNumber);
          atom.sourceLine = lineNumber;
          result.molecule().addAtom(atom);
          if (atom.serialNumber != 0)
            atomIndexBySerial[atom.serialNumber] = result.molecule().atoms().size() - 1;
        } catch (const invalid_argument&) {
          addWarning(result, "ATOM/HETATM record has invalid numeric coordinate data", lineNumber);
        } catch (const out_of_range&) {
          addWarning(result, "ATOM/HETATM record has out-of-range numeric data", lineNumber);
        }
      } else if (record == "CONECT") {
        try {
          const auto serials = parsePdbConectSerials(line);
          if (serials.size() < 2)
            continue;
          for (size_t i = 1; i < serials.size(); ++i)
            conectRecords.emplace_back(serials[0], serials[i]);
        } catch (const invalid_argument&) {
          addWarning(result, "CONECT record has invalid atom serial data", lineNumber);
        } catch (const out_of_range&) {
          addWarning(result, "CONECT record has out-of-range atom serial data", lineNumber);
        }
      } else if (record == "ANISOU" || record == "TER" || record == "END" || record == "REMARK" ||
                 record == "SEQRES" || record.empty()) {
        continue;
      } else if (!record.empty()) {
        addWarning(result, "unsupported PDB record: " + record, lineNumber);
      }
    }

    for (const auto& [firstSerial, secondSerial] : conectRecords) {
      const auto first = atomIndexBySerial.find(firstSerial);
      const auto second = atomIndexBySerial.find(secondSerial);
      if (first != atomIndexBySerial.end() && second != atomIndexBySerial.end())
        result.molecule().addBond(first->second, second->second);
    }

    return result;
  }

  MoleculeParseResult MoleculeParser::parseMmcif(istream& input) const {
    MoleculeParseResult result;
    const auto tokens = tokenizeCif(input);
    map<AtomSiteKey, size_t> atomIndexBySite;
    set<AtomSiteKey> acceptedAtomSites;
    vector<MmcifConnection> connections;

    for (size_t i = 0; i < tokens.size();) {
      const auto& token = tokens[i];
      if (startsWith(token.text, "data_")) {
        result.molecule().metadata().id = token.text.substr(5);
        ++i;
      } else if (token.text == "loop_") {
        ++i;
        vector<Token> tags;
        while (i < tokens.size() && startsWith(tokens[i].text, "_")) {
          tags.push_back(tokens[i]);
          ++i;
        }

        const bool isAtomSiteLoop = any_of(tags.begin(), tags.end(), [](const Token& tag) {
          return startsWith(tag.text, "_atom_site.");
        });
        const bool isStructConnLoop = any_of(tags.begin(), tags.end(), [](const Token& tag) {
          return startsWith(tag.text, "_struct_conn.");
        });
        if (tags.empty()) {
          addWarning(result, "mmCIF loop_ has no tags", token.line);
          continue;
        }

        while (i < tokens.size() && tokens[i].text != "loop_" &&
               !startsWith(tokens[i].text, "data_") && !startsWith(tokens[i].text, "_")) {
          if (i + tags.size() > tokens.size()) {
            addWarning(result, "mmCIF loop row has too few values", tokens[i].line);
            i = tokens.size();
            break;
          }

          if (isAtomSiteLoop || isStructConnLoop) {
            map<string, string> row;
            const auto rowLine = tokens[i].line;
            for (size_t tagIndex = 0; tagIndex < tags.size(); ++tagIndex)
              row[tags[tagIndex].text] = tokens[i + tagIndex].text;

            if (isAtomSiteLoop) {
              try {
                const auto x = parseOptionalDouble(valueFor(row, "_atom_site.Cartn_x"));
                const auto y = parseOptionalDouble(valueFor(row, "_atom_site.Cartn_y"));
                const auto z = parseOptionalDouble(valueFor(row, "_atom_site.Cartn_z"));
                if (!x || !y || !z) {
                  addWarning(result, "atom_site row is missing one or more coordinates", rowLine);
                } else {
                  const auto siteKey = atomSiteKeyFromRow(row);
                  const auto altId = normalizedAltId(valueFor(row, "_atom_site.label_alt_id"));
                  if (!altId.empty() &&
                      acceptedAtomSites.find(siteKey) != acceptedAtomSites.end()) {
                    addWarning(
                      result,
                      "atom_site alternate location '" + altId +
                        "' skipped; keeping the blank or first-seen alternate for this atom site",
                      rowLine);
                  } else if (acceptedAtomSites.find(siteKey) != acceptedAtomSites.end()) {
                    addWarning(result,
                               "duplicate atom_site row skipped for an already accepted atom site",
                               rowLine);
                  } else {
                    Atom atom;
                    atom.hetero = valueFor(row, "_atom_site.group_PDB") == "HETATM";
                    atom.serialNumber =
                      parseOptionalInt(valueFor(row, "_atom_site.id")).value_or(0);
                    atom.name = siteKey.atomName;
                    atom.alternateLocation = altId;
                    atom.element = valueFor(row, "_atom_site.type_symbol");
                    atom.residueName = siteKey.residueName;
                    atom.chainId = siteKey.chainId;
                    atom.residueSequence = siteKey.residueSequence;
                    atom.insertionCode = siteKey.insertionCode;
                    atom.position = Vector3d(*x, *y, *z);
                    atom.occupancy = parseOptionalDouble(valueFor(row, "_atom_site.occupancy"));
                    atom.temperatureFactor =
                      parseOptionalDouble(valueFor(row, "_atom_site.B_iso_or_equiv"));
                    atom.modelId = siteKey.modelId;
                    atom.sourceRecord =
                      valueFor(row, "_atom_site.group_PDB") + " " + to_string(atom.serialNumber);
                    atom.sourceLine = rowLine;
                    result.molecule().addAtom(atom);
                    acceptedAtomSites.insert(siteKey);
                    atomIndexBySite[siteKey] = result.molecule().atoms().size() - 1;
                  }
                }
              } catch (const invalid_argument&) {
                addWarning(result, "atom_site row has invalid numeric coordinate data", rowLine);
              } catch (const out_of_range&) {
                addWarning(result, "atom_site row has out-of-range numeric data", rowLine);
              }
            } else if (isStructConnLoop) {
              try {
                auto first = connectionPartnerKeyFromRow(row, "ptnr1");
                auto second = connectionPartnerKeyFromRow(row, "ptnr2");
                if (first.atomName.empty() || second.atomName.empty()) {
                  addWarning(result, "struct_conn row is missing one or more atom identifiers",
                             rowLine);
                } else {
                  connections.push_back(MmcifConnection{
                    first, second, bondOrderForConnType(valueFor(row, "_struct_conn.conn_type_id")),
                    rowLine});
                }
              } catch (const invalid_argument&) {
                addWarning(result, "struct_conn row has invalid numeric atom identifier data",
                           rowLine);
              } catch (const out_of_range&) {
                addWarning(result, "struct_conn row has out-of-range atom identifier data",
                           rowLine);
              }
            }
          }

          i += tags.size();
        }
      } else if (token.text == "_struct.title" && i + 1 < tokens.size()) {
        result.molecule().metadata().title = normalizeCifValue(tokens[i + 1].text);
        i += 2;
      } else {
        i += startsWith(token.text, "_") && i + 1 < tokens.size() ? 2 : 1;
      }
    }

    for (const auto& connection : connections) {
      const auto first = atomIndexBySite.find(connection.first);
      const auto second = atomIndexBySite.find(connection.second);
      if (first == atomIndexBySite.end() || second == atomIndexBySite.end()) {
        addWarning(result, "struct_conn row references atoms that were not imported",
                   connection.line);
        continue;
      }
      result.molecule().addBond(first->second, second->second, connection.order);
    }

    return result;
  }

}
