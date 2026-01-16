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

#ifndef MERMAID_H
#define MERMAID_H

#include <map>
#include <string>

#include "containers.h"
#include "qcstring.h"

#define MERMAID_DIVIDE_COUNT   4
#define MIN_MERMAID_COUNT      8

class QCString;

/** Content structure for Mermaid diagrams */
struct MermaidContent
{
  MermaidContent(const QCString &content_, const QCString &outDir_,
                 const QCString &srcFile_, int srcLine_)
    : content(content_), outDir(outDir_), srcFile(srcFile_), srcLine(srcLine_) {}
  QCString content;
  QCString outDir;
  QCString srcFile;
  int srcLine;
};

/** Singleton that manages Mermaid diagram generation */
class MermaidManager
{
  public:
    /** Mermaid output image formats */
    enum OutputFormat { MERMAID_SVG, MERMAID_PNG, MERMAID_PDF };

    /** Returns the singleton instance */
    static MermaidManager &instance();

    /** Run mermaid-cli tool for all diagrams */
    void run();

    /** Write a Mermaid diagram source file.
     *  @param[in] outDirArg   the output directory to write the file to.
     *  @param[in] fileName    the name of the file. If empty a name will be chosen automatically.
     *  @param[in] content     the contents of the Mermaid diagram.
     *  @param[in] format      the image format to generate.
     *  @param[in] srcFile     the source file resulting in the write command.
     *  @param[in] srcLine     the line number resulting in the write command.
     *  @param[in] inlineCode  true if code is from @startmermaid/@endmermaid,
     *                         false if from @mermaidfile command.
     *  @returns The names of the generated files.
     */
    StringVector writeMermaidSource(const QCString &outDirArg, const QCString &fileName,
                                    const QCString &content, OutputFormat format,
                                    const QCString &srcFile, int srcLine,
                                    bool inlineCode);

    /** Convert a Mermaid file to an image.
     *  @param[in] baseName the name of the generated file (as returned by writeMermaidSource())
     *  @param[in] outDir   the directory to write the resulting image into.
     *  @param[in] format   the image format to generate.
     */
    void generateMermaidOutput(const QCString &baseName, const QCString &outDir,
                               OutputFormat format);

    /** Get the output format based on configuration */
    static OutputFormat getOutputFormat();

    /** Get file extension for a given format */
    static QCString getExtension(OutputFormat format);

    using FilesMap   = std::map<std::string, StringVector>;
    using ContentMap = std::map<std::string, MermaidContent>;

  private:
    MermaidManager();

    void insert(const std::string &key,
                const std::string &value,
                const QCString &outDir,
                OutputFormat format,
                const QCString &mmdContent,
                const QCString &srcFile,
                int srcLine);

    void generateMermaidFileNames(const QCString &fileName, OutputFormat format,
                                  const QCString &outDir,
                                  QCString &baseName, QCString &mmdName,
                                  QCString &imgName);

    QCString computeMd5(const QCString &content);

    FilesMap   m_svgMermaidFiles;
    FilesMap   m_pngMermaidFiles;
    FilesMap   m_pdfMermaidFiles;
    ContentMap m_svgMermaidContent;
    ContentMap m_pngMermaidContent;
    ContentMap m_pdfMermaidContent;
};

#endif // MERMAID_H
