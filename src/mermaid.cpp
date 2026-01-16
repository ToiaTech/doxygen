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

#include <mutex>
#include <fstream>

#include "mermaid.h"
#include "util.h"
#include "portable.h"
#include "config.h"
#include "doxygen.h"
#include "message.h"
#include "debug.h"
#include "fileinfo.h"
#include "dir.h"
#include "indexlist.h"
#include "md5.h"

static std::mutex g_MermaidMutex;

//--------------------------------------------------------------------

MermaidManager &MermaidManager::instance()
{
  static MermaidManager theInstance;
  return theInstance;
}

MermaidManager::MermaidManager()
{
}

//--------------------------------------------------------------------

MermaidManager::OutputFormat MermaidManager::getOutputFormat()
{
  QCString format = Config_getEnumAsString(MERMAID_FORMAT);
  if (format == "png") return MERMAID_PNG;
  if (format == "pdf") return MERMAID_PDF;
  return MERMAID_SVG; // default
}

QCString MermaidManager::getExtension(OutputFormat format)
{
  switch (format)
  {
    case MERMAID_PNG: return ".png";
    case MERMAID_PDF: return ".pdf";
    case MERMAID_SVG:
    default:          return ".svg";
  }
}

//--------------------------------------------------------------------

QCString MermaidManager::computeMd5(const QCString &content)
{
  uint8_t md5_sig[16];
  MD5Buffer(content.data(),
            static_cast<unsigned int>(content.length()), md5_sig);
  char sigStr[33];
  MD5SigToString(md5_sig, sigStr);
  return QCString(sigStr);
}

//--------------------------------------------------------------------

void MermaidManager::generateMermaidFileNames(const QCString &fileName,
                                               OutputFormat format,
                                               const QCString &outDir,
                                               QCString &baseName,
                                               QCString &mmdName,
                                               QCString &imgName)
{
  static int mermaidIndex = 1;

  if (fileName.isEmpty()) // generate name
  {
    std::lock_guard<std::mutex> lock(g_MermaidMutex);
    mmdName = "inline_mermaid_" + QCString().setNum(mermaidIndex);
    baseName = outDir + "/inline_mermaid_" + QCString().setNum(mermaidIndex++);
  }
  else // user specified name
  {
    baseName = fileName;
    int i = baseName.findRev('.');
    if (i != -1) baseName = baseName.left(i);
    mmdName = baseName;
    baseName.prepend(outDir + "/");
  }

  imgName = mmdName + getExtension(format);
}

//--------------------------------------------------------------------

StringVector MermaidManager::writeMermaidSource(const QCString &outDirArg,
                                                 const QCString &fileName,
                                                 const QCString &content,
                                                 OutputFormat format,
                                                 const QCString &srcFile,
                                                 int srcLine,
                                                 bool inlineCode)
{
  StringVector baseNameVector;
  QCString baseName;
  QCString mmdName;
  QCString imgName;
  QCString outDir(outDirArg);

  Debug::print(Debug::Mermaid, 0, "*** writeMermaidSource fileName: {}\n", fileName);
  Debug::print(Debug::Mermaid, 0, "*** writeMermaidSource outDir: {}\n", outDir);

  // strip any trailing slashes and backslashes
  size_t l = 0;
  while ((l = outDir.length()) > 0 && (outDir.at(l-1) == '/' || outDir.at(l-1) == '\\'))
  {
    outDir = outDir.left(l-1);
  }

  generateMermaidFileNames(fileName, format, outDir, baseName, mmdName, imgName);

  Debug::print(Debug::Mermaid, 0, "*** writeMermaidSource baseName: {}\n", baseName);
  Debug::print(Debug::Mermaid, 0, "*** writeMermaidSource mmdName: {}\n", mmdName);
  Debug::print(Debug::Mermaid, 0, "*** writeMermaidSource imgName: {}\n", imgName);

  // For mermaid, content is already the diagram definition
  QCString text = content;
  if (!text.endsWith("\n"))
  {
    text += '\n';
  }

  QCString qcOutDir(substitute(outDir, "\\", "/"));
  uint32_t pos = qcOutDir.findRev("/");
  QCString generateType(qcOutDir.right(qcOutDir.length() - (pos + 1)));

  Debug::print(Debug::Mermaid, 0, "*** writeMermaidSource generateType: {}\n", generateType);

  insert(generateType.str(), mmdName.str(), outDir, format, text, srcFile, srcLine);
  baseNameVector.push_back(baseName.str());

  return baseNameVector;
}

//--------------------------------------------------------------------

void MermaidManager::generateMermaidOutput(const QCString &baseName,
                                            const QCString & /* outDir */,
                                            OutputFormat format)
{
  QCString imgName = baseName;
  // The basename contains path, we need to strip the path from the filename
  // in order to create the image file name for the index.
  int i = imgName.findRev('/');
  if (i != -1) // strip path
  {
    imgName = imgName.mid(i+1);
  }
  imgName += getExtension(format);

  Doxygen::indexList->addImageFile(imgName);
}

//--------------------------------------------------------------------

static void addMermaidFiles(MermaidManager::FilesMap &mermaidFiles,
                            const std::string &key, const std::string &value)
{
  auto kv = mermaidFiles.find(key);
  if (kv == mermaidFiles.end())
  {
    kv = mermaidFiles.emplace(key, StringVector()).first;
  }
  kv->second.push_back(value);
}

static void addMermaidContent(MermaidManager::ContentMap &mermaidContent,
                              const std::string &key, const QCString &outDir,
                              const QCString &mmdContent,
                              const QCString &srcFile, int srcLine)
{
  auto kv = mermaidContent.find(key);
  if (kv == mermaidContent.end())
  {
    kv = mermaidContent.emplace(key, MermaidContent("", outDir, srcFile, srcLine)).first;
  }
  kv->second.content += mmdContent;
}

//--------------------------------------------------------------------

void MermaidManager::insert(const std::string &key, const std::string &value,
                            const QCString &outDir, OutputFormat format,
                            const QCString &mmdContent,
                            const QCString &srcFile, int srcLine)
{
  Debug::print(Debug::Mermaid, 0, "*** MermaidManager::insert key:{}, value:{}\n", key, value);

  switch (format)
  {
    case MERMAID_PNG:
      addMermaidFiles(m_pngMermaidFiles, key, value);
      addMermaidContent(m_pngMermaidContent, key, outDir, mmdContent, srcFile, srcLine);
      break;
    case MERMAID_PDF:
      addMermaidFiles(m_pdfMermaidFiles, key, value);
      addMermaidContent(m_pdfMermaidContent, key, outDir, mmdContent, srcFile, srcLine);
      break;
    case MERMAID_SVG:
      addMermaidFiles(m_svgMermaidFiles, key, value);
      addMermaidContent(m_svgMermaidContent, key, outDir, mmdContent, srcFile, srcLine);
      break;
  }
}

//--------------------------------------------------------------------

static void runMermaidContent(const MermaidManager::FilesMap &mermaidFiles,
                              const MermaidManager::ContentMap &mermaidContent,
                              MermaidManager::OutputFormat format)
{
  if (Doxygen::verifiedMermaidPath.isEmpty())
  {
    return; // mermaid not configured
  }

  int exitCode = 0;
  QCString mermaidExe = Doxygen::verifiedMermaidPath;
  QCString mermaidConfigFile = Config_getString(MERMAID_CONFIG_FILE);
  QCString mermaidTheme = Config_getEnumAsString(MERMAID_THEME);

  // Map 'base' theme to mermaid's 'default'
  if (mermaidTheme == "base")
  {
    mermaidTheme = "default";
  }

  QCString formatStr;
  switch (format)
  {
    case MermaidManager::MERMAID_PNG: formatStr = "png"; break;
    case MermaidManager::MERMAID_PDF: formatStr = "pdf"; break;
    case MermaidManager::MERMAID_SVG:
    default:                          formatStr = "svg"; break;
  }

  for (const auto &[name, nb] : mermaidContent)
  {
    if (nb.content.isEmpty()) continue;

    msg("Generating Mermaid {} files in {}\n", formatStr, name);

    // Write each diagram to a separate file and invoke mmdc
    QCString mmdOutDir = nb.outDir;
    auto files_kv = mermaidFiles.find(name);
    if (files_kv == mermaidFiles.end()) continue;

    // We need to split content by diagram since mermaid processes one file at a time
    // For now, we write all content to one file and let mermaid handle it
    // Actually, we need to handle each diagram separately

    QCString mmdFileName = mmdOutDir + "/inline_mermaid_" + formatStr + name.c_str() + ".mmd";
    QCString imgFileName = mmdOutDir + "/inline_mermaid_" + formatStr + name.c_str() + "." + formatStr;

    // Check if content has changed using MD5
    uint8_t md5_sig[16];
    MD5Buffer(nb.content.data(),
              static_cast<unsigned int>(nb.content.length()), md5_sig);
    char md5SigStr[33];
    MD5SigToString(md5_sig, md5SigStr);
    QCString md5Hash(md5SigStr);
    QCString md5FileName = mmdOutDir + "/inline_mermaid_" + formatStr + name.c_str() + ".md5";

    QCString cachedMd5;
    FileInfo md5fi(md5FileName.str());
    if (md5fi.exists())
    {
      cachedMd5 = fileToString(md5FileName);
      cachedMd5 = cachedMd5.stripWhiteSpace();
    }

    // Write the mermaid source file
    std::ofstream file = Portable::openOutputStream(mmdFileName);
    if (!file.is_open())
    {
      err_full(nb.srcFile, nb.srcLine, "Could not open file {} for writing", mmdFileName);
      continue;
    }
    file.write(nb.content.data(), nb.content.length());
    file.close();

    // Skip regeneration if content hasn't changed
    if (cachedMd5 == md5Hash)
    {
      Debug::print(Debug::Mermaid, 0, "*** MermaidManager: Skipping {} (unchanged)\n", mmdFileName);
      continue;
    }

    // Build mmdc command arguments
    QCString mermaidArgs;
    mermaidArgs += "-i \"" + mmdFileName + "\" ";
    mermaidArgs += "-o \"" + imgFileName + "\" ";
    mermaidArgs += "-e " + formatStr + " ";

    if (!mermaidTheme.isEmpty())
    {
      mermaidArgs += "-t " + mermaidTheme + " ";
    }

    if (!mermaidConfigFile.isEmpty())
    {
      FileInfo cfgFi(mermaidConfigFile.str());
      if (cfgFi.exists())
      {
        mermaidArgs += "-c \"" + mermaidConfigFile + "\" ";
      }
    }

    Debug::print(Debug::Mermaid, 0, "*** MermaidManager::run Running: {} {}\n",
                 mermaidExe, mermaidArgs);

    if ((exitCode = Portable::system(mermaidExe.data(), mermaidArgs.data(), TRUE)) != 0)
    {
      err_full(nb.srcFile, nb.srcLine,
               "Problems running mermaid-cli (mmdc). Verify that '{}' works from the command line. Exit code: {}.",
               mermaidExe, exitCode);
    }
    else
    {
      // Write MD5 cache file on success
      std::ofstream md5file = Portable::openOutputStream(md5FileName);
      if (md5file.is_open())
      {
        md5file << md5Hash.str();
        md5file.close();
      }
    }

    // Cleanup .mmd file if DOT_CLEANUP is enabled
    if (Config_getBool(DOT_CLEANUP))
    {
      Dir().remove(mmdFileName.str());
      Dir().remove(md5FileName.str());
    }
  }
}

//--------------------------------------------------------------------

void MermaidManager::run()
{
  Debug::print(Debug::Mermaid, 0, "*** MermaidManager::run\n");
  runMermaidContent(m_pngMermaidFiles, m_pngMermaidContent, MERMAID_PNG);
  runMermaidContent(m_svgMermaidFiles, m_svgMermaidContent, MERMAID_SVG);
  runMermaidContent(m_pdfMermaidFiles, m_pdfMermaidContent, MERMAID_PDF);
}

//--------------------------------------------------------------------
