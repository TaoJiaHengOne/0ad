/* Copyright (C) 2025 Wildfire Games.
 * This file is part of 0 A.D.
 *
 * 0 A.D. is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * 0 A.D. is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with 0 A.D.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "precompiled.h"

#include "ArchiveBuilder.h"

#include "graphics/TextureManager.h"
#include "graphics/ColladaManager.h"
#include "lib/tex/tex_codec.h"
#include "lib/file/archive/archive_zip.h"
#include "lib/file/vfs/vfs_util.h"
#include "ps/XML/Xeromyces.h"
#include "renderer/backend/dummy/Device.h"

#include <boost/algorithm/string/predicate.hpp>

CArchiveBuilder::CArchiveBuilder(const OsPath& mod, const OsPath& tempdir) :
	m_TempDir(tempdir), m_NumBaseMods(0)
{
	m_VFS = CreateVfs();

	DeleteDirectory(m_TempDir/"_archivecache"); // clean up in case the last run failed

	m_VFS->Mount(L"cache/", m_TempDir/"_archivecache"/"", 0, VFS_MAX_PRIORITY);

	// Mount with highest priority so base mods do not overwrite files in this mod
	m_VFS->Mount(L"", mod/"", VFS_MOUNT_MUST_EXIST | VFS_MOUNT_KEEP_DELETED, VFS_MAX_PRIORITY);

	// Collect the list of files before loading any base mods
	vfs::ForEachFile(m_VFS, L"", &CollectFileCB, (uintptr_t)static_cast<void*>(this), 0, vfs::DIR_RECURSIVE);
}

CArchiveBuilder::~CArchiveBuilder()
{
	m_VFS.reset();

	DeleteDirectory(m_TempDir/"_archivecache");
}

void CArchiveBuilder::AddBaseMod(const OsPath& mod)
{
	// Increase priority for each additional base mod, so that the
	// mods are mounted in the same way as when starting the game.
	m_VFS->Mount(L"", mod/"", VFS_MOUNT_MUST_EXIST, ++m_NumBaseMods);
}

void CArchiveBuilder::Build(const OsPath& archive, bool compress)
{
	// By default we disable zip compression because it significantly hurts download
	// size for releases (which re-compress all files with better compression
	// algorithms) - it's probably most important currently to optimise for
	// download size rather than install size or startup performance.
	// (See https://gitea.wildfiregames.com/0ad/0ad/issues/671)
	const bool noDeflate = !compress;

	PIArchiveWriter writer = CreateArchiveWriter_Zip(archive, noDeflate);
	if (!writer)
	{
		debug_printf("Failed to create the archive \"%s\".", archive.string8().c_str());
		return;
	}

	// Use CTextureManager instead of CTextureConverter directly,
	// so it can deal with all the loading of settings.xml files
	Renderer::Backend::Dummy::CDevice device;
	CTextureManager textureManager(m_VFS, true, &device);

	CColladaManager colladaManager(m_VFS);

	CXeromyces xero;

	for (const VfsPath& path : m_Files)
	{
		Status ret;
		OsPath realPath;
		ret = m_VFS->GetRealPath(path, realPath);
		ENSURE(ret == INFO::OK);

		// Compress textures and store the new cached version instead of the original
		if ((boost::algorithm::starts_with(path.string(), L"art/textures/") ||
			 boost::algorithm::starts_with(path.string(), L"fonts/")
			) &&
			tex_is_known_extension(path) &&
			// Skip some subdirectories where the engine doesn't use CTextureManager yet:
			!boost::algorithm::starts_with(path.string(), L"art/textures/cursors/") &&
			!boost::algorithm::starts_with(path.string(), L"art/textures/terrain/alphamaps/")
		)
		{
			VfsPath cachedPath;
			debug_printf("Converting texture \"%s\"\n", realPath.string8().c_str());
			bool ok = textureManager.GenerateCachedTexture(path, cachedPath);
			ENSURE(ok);

			OsPath cachedRealPath;
			ret = m_VFS->GetRealPath(VfsPath("cache")/cachedPath, cachedRealPath);
			ENSURE(ret == INFO::OK);

			writer->AddFile(cachedRealPath, cachedPath);

			// We don't want to store the original file too (since it's a
			// large waste of space), so skip to the next file
			continue;
		}

		// Convert DAE models and store the new cached version instead of the original
		if (path.Extension() == L".dae")
		{
			CColladaManager::FileType type;

			if (boost::algorithm::starts_with(path.string(), L"art/meshes/"))
				type = CColladaManager::PMD;
			else if (boost::algorithm::starts_with(path.string(), L"art/animation/"))
				type = CColladaManager::PSA;
			else
			{
				// Unknown type of DAE, just add to archive and continue
				writer->AddFile(realPath, path);
				continue;
			}

			VfsPath cachedPath;
			debug_printf("Converting model %s\n", realPath.string8().c_str());
			bool ok = colladaManager.GenerateCachedFile(path, type, cachedPath);

			// The DAE might fail to convert for whatever reason, and in that case
			//	it can't be used in the game, so we just exclude it
			//  (alternatively we could throw release blocking errors on useless files)
			if (ok)
			{
				OsPath cachedRealPath;
				ret = m_VFS->GetRealPath(VfsPath("cache")/cachedPath, cachedRealPath);
				ENSURE(ret == INFO::OK);

				writer->AddFile(cachedRealPath, cachedPath);
			}

			// We don't want to store the original file too (since it's a
			// large waste of space), so skip to the next file
			continue;
		}

		debug_printf("Adding %s\n", realPath.string8().c_str());
		writer->AddFile(realPath, path);

		// Also cache XMB versions of all XML files
		if (path.Extension() == L".xml")
		{
			VfsPath cachedPath;
			debug_printf("Converting XML file \"%s\"\n", realPath.string8().c_str());
			bool ok = xero.GenerateCachedXMB(m_VFS, path, cachedPath);
			ENSURE(ok);

			OsPath cachedRealPath;
			ret = m_VFS->GetRealPath(VfsPath("cache")/cachedPath, cachedRealPath);
			ENSURE(ret == INFO::OK);

			writer->AddFile(cachedRealPath, cachedPath);
		}
	}

	debug_printf("Finished packaging \"%s\".", archive.string8().c_str());
}

Status CArchiveBuilder::CollectFileCB(const VfsPath& pathname, const CFileInfo&, const uintptr_t cbData)
{
	CArchiveBuilder* self = static_cast<CArchiveBuilder*>((void*)cbData);
	self->m_Files.push_back(pathname);

	return INFO::OK;
}
