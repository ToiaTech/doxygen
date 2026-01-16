/******************************************************************************
 *
 * Copyright (C) 1997-2025 by Dimitri van Heesch.
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation under the terms of the GNU General Public License is hereby
 * granted. No representations are made about the suitability of this software
 * for any purpose. It is provided "as is" without express or implied warranty.
 * See the GNU General Public License for more details.
 *
 * Documents produced by Doxygen are derivative works derived from the
 * input used in their production; they are not affected by this license.
 *
 */

#include "mermaidgraph.h"
#include "textstream.h"
#include "util.h"

//--------------------------------------------------------------------

void MermaidGraph::writeHeader(TextStream &t, DiagramType type, const QCString &title)
{
  switch (type)
  {
    case ClassDiagram:
      t << "classDiagram\n";
      break;
    case FlowchartLR:
      t << "flowchart LR\n";
      break;
    case FlowchartTD:
      t << "flowchart TD\n";
      break;
    case FlowchartRL:
      t << "flowchart RL\n";
      break;
    case FlowchartBT:
      t << "flowchart BT\n";
      break;
  }
  if (!title.isEmpty())
  {
    t << "    %% " << title << "\n";
  }
}

//--------------------------------------------------------------------

void MermaidGraph::writeFooter(TextStream &t)
{
  // Mermaid diagrams don't require an explicit footer
  t << "\n";
}

//--------------------------------------------------------------------

void MermaidGraph::getShapeDelimiters(NodeShape shape, QCString &open, QCString &close)
{
  switch (shape)
  {
    case Box:
      open = "[";
      close = "]";
      break;
    case RoundedBox:
      open = "(";
      close = ")";
      break;
    case Stadium:
      open = "([";
      close = "])";
      break;
    case Diamond:
      open = "{";
      close = "}";
      break;
    case Hexagon:
      open = "{{";
      close = "}}";
      break;
    case Circle:
      open = "((";
      close = "))";
      break;
    case Asymmetric:
      open = ">";
      close = "]";
      break;
    case Database:
      open = "[(";
      close = ")]";
      break;
  }
}

//--------------------------------------------------------------------

QCString MermaidGraph::escapeLabel(const QCString &label)
{
  QCString result;
  const char *p = label.data();
  if (p == nullptr) return result;

  while (*p)
  {
    switch (*p)
    {
      case '"':  result += "&quot;"; break;
      case '<':  result += "&lt;"; break;
      case '>':  result += "&gt;"; break;
      case '&':  result += "&amp;"; break;
      case '#':  result += "&num;"; break;
      case '\n': result += "<br/>"; break;
      default:   result += *p;
    }
    p++;
  }
  return result;
}

//--------------------------------------------------------------------

QCString MermaidGraph::escapeId(const QCString &id)
{
  QCString result;
  const char *p = id.data();
  if (p == nullptr) return result;

  while (*p)
  {
    // Only allow alphanumeric and underscore in IDs
    if ((*p >= 'a' && *p <= 'z') ||
        (*p >= 'A' && *p <= 'Z') ||
        (*p >= '0' && *p <= '9') ||
        *p == '_')
    {
      result += *p;
    }
    else
    {
      // Replace other chars with underscore
      result += '_';
    }
    p++;
  }
  return result;
}

//--------------------------------------------------------------------

void MermaidGraph::writeNode(TextStream &t,
                             const QCString &id,
                             const QCString &label,
                             NodeShape shape)
{
  QCString open, close;
  getShapeDelimiters(shape, open, close);

  t << "    " << escapeId(id) << open << "\"" << escapeLabel(label) << "\"" << close << "\n";
}

//--------------------------------------------------------------------

void MermaidGraph::writeEdge(TextStream &t,
                             const QCString &fromId,
                             const QCString &toId,
                             const QCString &label,
                             bool dashed)
{
  t << "    " << escapeId(fromId);

  if (dashed)
  {
    if (label.isEmpty())
    {
      t << " -.-> ";
    }
    else
    {
      t << " -. \"" << escapeLabel(label) << "\" .-> ";
    }
  }
  else
  {
    if (label.isEmpty())
    {
      t << " --> ";
    }
    else
    {
      t << " -- \"" << escapeLabel(label) << "\" --> ";
    }
  }

  t << escapeId(toId) << "\n";
}

//--------------------------------------------------------------------

void MermaidGraph::writeClassNode(TextStream &t,
                                  const QCString &className,
                                  const StringVector &members,
                                  const StringVector &methods)
{
  t << "    class " << escapeId(className) << " {\n";

  for (const auto &member : members)
  {
    t << "        " << escapeLabel(member) << "\n";
  }

  for (const auto &method : methods)
  {
    t << "        " << escapeLabel(method) << "()\n";
  }

  t << "    }\n";
}

//--------------------------------------------------------------------

void MermaidGraph::writeInheritance(TextStream &t,
                                    const QCString &child,
                                    const QCString &parent)
{
  // In Mermaid class diagrams, inheritance is Parent <|-- Child
  t << "    " << escapeId(parent) << " <|-- " << escapeId(child) << "\n";
}

//--------------------------------------------------------------------

void MermaidGraph::writeAssociation(TextStream &t,
                                    const QCString &from,
                                    const QCString &to,
                                    const QCString &label)
{
  t << "    " << escapeId(from);
  if (label.isEmpty())
  {
    t << " --> ";
  }
  else
  {
    t << " --> \"" << escapeLabel(label) << "\" ";
  }
  t << escapeId(to) << "\n";
}

//--------------------------------------------------------------------

void MermaidGraph::writeAggregation(TextStream &t,
                                    const QCString &container,
                                    const QCString &contained,
                                    const QCString &label)
{
  // Aggregation: container o-- contained (diamond at container end)
  t << "    " << escapeId(container);
  if (label.isEmpty())
  {
    t << " o-- ";
  }
  else
  {
    t << " o-- \"" << escapeLabel(label) << "\" ";
  }
  t << escapeId(contained) << "\n";
}

//--------------------------------------------------------------------

void MermaidGraph::writeComposition(TextStream &t,
                                    const QCString &container,
                                    const QCString &contained,
                                    const QCString &label)
{
  // Composition: container *-- contained (filled diamond at container end)
  t << "    " << escapeId(container);
  if (label.isEmpty())
  {
    t << " *-- ";
  }
  else
  {
    t << " *-- \"" << escapeLabel(label) << "\" ";
  }
  t << escapeId(contained) << "\n";
}

//--------------------------------------------------------------------

void MermaidGraph::writeSubgraphStart(TextStream &t,
                                      const QCString &id,
                                      const QCString &label)
{
  t << "    subgraph " << escapeId(id) << "[\"" << escapeLabel(label) << "\"]\n";
}

//--------------------------------------------------------------------

void MermaidGraph::writeSubgraphEnd(TextStream &t)
{
  t << "    end\n";
}

//--------------------------------------------------------------------

void MermaidGraph::writeStyle(TextStream &t,
                              const QCString &nodeId,
                              const QCString &fill,
                              const QCString &stroke,
                              const QCString &strokeWidth)
{
  t << "    style " << escapeId(nodeId) << " fill:" << fill << ",stroke:" << stroke;
  if (!strokeWidth.isEmpty())
  {
    t << ",stroke-width:" << strokeWidth;
  }
  t << "\n";
}

//--------------------------------------------------------------------

void MermaidGraph::writeLink(TextStream &t,
                             const QCString &nodeId,
                             const QCString &url,
                             const QCString &tooltip)
{
  t << "    click " << escapeId(nodeId) << " \"" << url << "\"";
  if (!tooltip.isEmpty())
  {
    t << " \"" << escapeLabel(tooltip) << "\"";
  }
  t << "\n";
}

//--------------------------------------------------------------------
