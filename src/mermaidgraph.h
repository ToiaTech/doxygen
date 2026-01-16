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

#ifndef MERMAIDGRAPH_H
#define MERMAIDGRAPH_H

#include <vector>
#include "qcstring.h"
#include "containers.h"

class TextStream;

/** Helper class for generating Mermaid diagram syntax */
class MermaidGraph
{
  public:
    /** Types of Mermaid diagrams supported */
    enum DiagramType {
      ClassDiagram,   ///< Class hierarchy diagrams
      FlowchartLR,    ///< Left to right flowchart (call graphs)
      FlowchartTD,    ///< Top to down flowchart (include deps)
      FlowchartRL,    ///< Right to left flowchart (caller graphs)
      FlowchartBT     ///< Bottom to top flowchart
    };

    /** Node shapes for flowcharts */
    enum NodeShape {
      Box,            ///< Rectangle [label]
      RoundedBox,     ///< Rounded rectangle (label)
      Stadium,        ///< Stadium shape ([label])
      Diamond,        ///< Decision diamond {label}
      Hexagon,        ///< Hexagon {{label}}
      Circle,         ///< Circle ((label))
      Asymmetric,     ///< Asymmetric >label]
      Database        ///< Cylinder [(label)]
    };

    /** Write diagram header */
    static void writeHeader(TextStream &t, DiagramType type, const QCString &title = QCString());

    /** Write diagram footer */
    static void writeFooter(TextStream &t);

    /** Write a flowchart node */
    static void writeNode(TextStream &t,
                          const QCString &id,
                          const QCString &label,
                          NodeShape shape = Box);

    /** Write an edge between nodes */
    static void writeEdge(TextStream &t,
                          const QCString &fromId,
                          const QCString &toId,
                          const QCString &label = QCString(),
                          bool dashed = false);

    /** Write a class node for class diagrams */
    static void writeClassNode(TextStream &t,
                               const QCString &className,
                               const StringVector &members,
                               const StringVector &methods);

    /** Write inheritance relationship */
    static void writeInheritance(TextStream &t,
                                 const QCString &child,
                                 const QCString &parent);

    /** Write association relationship */
    static void writeAssociation(TextStream &t,
                                 const QCString &from,
                                 const QCString &to,
                                 const QCString &label = QCString());

    /** Write aggregation relationship */
    static void writeAggregation(TextStream &t,
                                 const QCString &container,
                                 const QCString &contained,
                                 const QCString &label = QCString());

    /** Write composition relationship */
    static void writeComposition(TextStream &t,
                                 const QCString &container,
                                 const QCString &contained,
                                 const QCString &label = QCString());

    /** Write a subgraph/cluster */
    static void writeSubgraphStart(TextStream &t,
                                   const QCString &id,
                                   const QCString &label);

    /** End a subgraph/cluster */
    static void writeSubgraphEnd(TextStream &t);

    /** Write a style definition */
    static void writeStyle(TextStream &t,
                          const QCString &nodeId,
                          const QCString &fill,
                          const QCString &stroke,
                          const QCString &strokeWidth = QCString());

    /** Write a click handler/link */
    static void writeLink(TextStream &t,
                          const QCString &nodeId,
                          const QCString &url,
                          const QCString &tooltip = QCString());

    /** Escape special characters in labels for Mermaid */
    static QCString escapeLabel(const QCString &label);

    /** Create a valid Mermaid node ID */
    static QCString escapeId(const QCString &id);

  private:
    /** Get shape delimiters for a given shape */
    static void getShapeDelimiters(NodeShape shape, QCString &open, QCString &close);
};

#endif // MERMAIDGRAPH_H
