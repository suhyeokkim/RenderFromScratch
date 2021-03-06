#include <DirectXTex.h>
#include <vector>
#include <d3d11_4.h>
#include <array>

#include "dx11res.h"
#include "dx11util.h"
#include "dx11depend.h"

#include "defined_alloc_macro.h"

HRESULT ReserveLoadInputLayoutRefIndex(
	DX11Resources* res, DX11InternalResourceDescBuffer* rawBuffer,
	uint descCount, const DX11CompileDescToShader* dtoss,
	uint inputLayoutCount, const DX11InputLayoutDesc* inputLayoutDesc
)
{
	ALLOC_RANGE_ZEROMEM(
		EASTL_TEMPARARY_NAME, res->inputLayoutCount, inputLayoutCount,
		DX11Resources::DX11InputLayout, res->inputLayouts
	);

	for (uint i = 0; i < res->inputLayoutCount; i++)
	{
		const DX11InputLayoutDesc& d = inputLayoutDesc[i];
		const DX11CompileDescToShader& dtof = dtoss[d.shaderCompileDescIndex];
		DX11ILDesc desc;
		desc.vertexLayoutChunkIndex = d.layoutChunkIndex;
		desc.vertexShaderIndex = dtof.shaderIndex;

		res->inputLayouts[i].inputLayoutIndex = ReserveLoadInputLayout(rawBuffer, desc);
		res->inputLayouts[i].vertexShaderIndex = dtof.shaderIndex;
		res->inputLayouts[i].layoutChunkIndex = d.layoutChunkIndex;
	}

	return S_OK;
}

HRESULT CreateDX11ResourcesByDesc(DX11Resources* res, DX11InternalResourceDescBuffer* rawBuffer, void* dataBuffer, ID3D11Device* device, bool isDebug)
{
	for (uint i = 0; i < 6; i++)
	{
		auto& shaderChunks = res->shadersByKind[i];
		auto& shaderDescs = rawBuffer->shaderCompileDesces[i];

		ALLOC_RANGE_ZEROMEM(
			EASTL_PERSISTANT_NAME, shaderChunks.shaderCount, rawBuffer->shaderCompileDesces[i].size(),
			DX11CompiledShader, shaderChunks.shaders
		);

		for (uint j = 0; j < shaderChunks.shaderCount; j++)
		{
			auto& desc = shaderDescs[j];
			auto& shader = shaderChunks.shaders[j];

			ALLOC_AND_STRCPY(EASTL_PERSISTANT_NAME, shader.target, desc.target);
			ALLOC_AND_STRCPY(EASTL_PERSISTANT_NAME, shader.entrypoint, desc.entrypoint);

			ID3DBlob* blob;
			FAILED_ERROR_MESSAGE_CONTINUE_ARGS(
				CompileShaderFromFile(desc.fileName, desc.entrypoint, desc.target, isDebug, &blob),
				L"fail to compile shader(%s,%s,%s)..",
				desc.fileName, desc.entrypoint, desc.target
			);
			shader.shaderBlob = blob;
			switch ((ShaderKind)i)
			{
			case ShaderKind::Vertex:
				FAILED_ERROR_MESSAGE_ARGS(
					device->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &shaderChunks.shaders[j].vs),
					L"fail to compile vertex shader(%s,%s,%s)..",
					desc.fileName, desc.entrypoint, desc.target
				);
				break;
			case ShaderKind::Pixel:
				FAILED_ERROR_MESSAGE_ARGS(
					device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &shaderChunks.shaders[j].ps),
					L"fail to compile pixel shader(%s,%s,%s)..",
					desc.fileName, desc.entrypoint, desc.target
				);
				break;
			case ShaderKind::Compute:
				FAILED_ERROR_MESSAGE_ARGS(
					device->CreateComputeShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &shaderChunks.shaders[j].cs),
					L"fail to compile compute shader(%s,%s,%s)..",
					desc.fileName, desc.entrypoint, desc.target
				);
				break;
			case ShaderKind::Geometry:
				FAILED_ERROR_MESSAGE_ARGS(
					device->CreateGeometryShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &shaderChunks.shaders[j].gs),
					L"fail to compile geometry shader(%s,%s,%s)..",
					desc.fileName, desc.entrypoint, desc.target
				);
				break;
			case ShaderKind::Hull:
				FAILED_ERROR_MESSAGE_ARGS(
					device->CreateHullShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &shaderChunks.shaders[j].hs),
					L"fail to compile hull shader(%s,%s,%s)..",
					desc.fileName, desc.entrypoint, desc.target
				);
				break;
			case ShaderKind::Doamin:
				FAILED_ERROR_MESSAGE_ARGS(
					device->CreateDomainShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &shaderChunks.shaders[j].ds),
					L"fail to compile domain shader(%s,%s,%s)..",
					desc.fileName, desc.entrypoint, desc.target
				);
				break;
			}

		}
	}

	ALLOC_RANGE_ZEROMEM(
		EASTL_PERSISTANT_NAME, res->samplerCount, rawBuffer->samplerDescs.size(),
		ID3D11SamplerState*, res->samplerStates
	);
	for (uint i = 0; i < res->samplerCount; i++)
	{
		FAILED_ERROR_MESSAGE_ARGS(
			device->CreateSamplerState(&rawBuffer->samplerDescs[i], &res->samplerStates[i]),
			L"failed to create sampler(idx:%d)..",
			i
		);
	}

	ALLOC_RANGE_ZEROMEM(
		EASTL_PERSISTANT_NAME, res->texture2DCount, rawBuffer->tex2DDescs.size(),
		ID3D11Texture2D*, res->texture2Ds
	);
	for (uint i = 0; i < res->texture2DCount; i++)
	{
		DX11Texture2DDesc& desc = rawBuffer->tex2DDescs[i];

		if (desc.loadFromFile)
		{
			if (!desc.fileName)
				ERROR_MESSAGE_CONTINUE_ARGS(L"fail to load texture because fileName is null..(idx:%d)", i);

			FILE* fp;
			_wfopen_s(&fp, desc.fileName, L"rb");
			size_t size = GetFileSize(fp);
			size_t readSize = fread(dataBuffer, 1, size, fp);
			fclose(fp);
			fp = nullptr;

			if (readSize != size)
				continue;

			DirectX::ScratchImage img;
			FAILED_WARN_MESSAGE_CONTINUE_ARGS(
				DirectX::LoadFromWICMemory(dataBuffer, size, DirectX::WIC_FLAGS::WIC_FLAGS_NONE, nullptr, img),
				L"fail to load image from file:\"%s\"..",
				desc.fileName
			);

			FAILED_WARN_MESSAGE_CONTINUE(
				DirectX::CreateTextureEx(
					device,
					img.GetImages(), img.GetImageCount(), img.GetMetadata(),
					desc.usage, desc.bindFlags, desc.cpuAccessFlags, desc.miscFlags, desc.forceSRGB,
					reinterpret_cast<ID3D11Resource**>(&res->texture2Ds[i])
				),
				L"fail to CreateTextureEx().."
			);
		}
		else
		{
			FAILED_WARN_MESSAGE_CONTINUE(
				device->CreateTexture2D(&desc.tex2DDesc, desc.subres.pSysMem? &desc.subres: nullptr, &res->texture2Ds[i]),
				L"fail to create d3d11 texture2d object.."
			);
		}
	}

	ALLOC_RANGE_ZEROMEM(
		EASTL_PERSISTANT_NAME, res->bufferCount, rawBuffer->bufferDescs.size(),
		ID3D11Buffer*, res->buffers
	);
	for (uint i = 0; i < res->bufferCount; i++)
	{
		DX11BufferDesc& d = rawBuffer->bufferDescs[i];

		if (d.subres.pSysMem == nullptr)
		{
			if ((bool)d.copyToPtr)
			{
				d.copyToPtr(dataBuffer);
				d.subres.pSysMem = dataBuffer;
			}
			else if (d.fileName)
			{
				FILE* fp;
				_wfopen_s(&fp, d.fileName, L"rb");
				size_t size = GetFileSize(fp);
				fread(dataBuffer, 1, size, fp);
				fclose(fp);
				d.subres.pSysMem = dataBuffer;
			}
		}

		FAILED_ERROR_MESSAGE_ARGS(
			device->CreateBuffer(&d.buffer, d.subres.pSysMem ? &d.subres : nullptr, &res->buffers[i]),
			L"fail to create buffer(idx:%d)..",
			i
		);
	}		

	ALLOC_RANGE_ZEROMEM(
		EASTL_PERSISTANT_NAME, res->srvCount, rawBuffer->srvDescs.size(),
		ID3D11ShaderResourceView*, res->srvs
	);
	for (uint i = 0; i < res->srvCount; i++)
	{
		ID3D11Resource* dx11res = nullptr;

		// TODO:: 텍스쳐 로드 확인 용 스핀락, 비동기 로드시 고쳐야함
		if ((bool)rawBuffer->srvDescs[i].setSRVDesc)
			while (!rawBuffer->srvDescs[i].setSRVDesc(&rawBuffer->srvDescs[i].view));

		switch (rawBuffer->srvDescs[i].view.ViewDimension)
		{
		case D3D11_SRV_DIMENSION::D3D11_SRV_DIMENSION_BUFFER:
			dx11res = res->buffers[rawBuffer->srvDescs[i].bufferIndex];
			break;
		case D3D11_SRV_DIMENSION::D3D11_SRV_DIMENSION_TEXTURE2D:
			dx11res = res->texture2Ds[rawBuffer->srvDescs[i].texture2DIndex];
			break;
		default:
			ERROR_MESSAGE_CONTINUE_ARGS(
				L"failed by corrupted view type:%d\n",
				rawBuffer->srvDescs[i].view.ViewDimension
			);
			break;
		}
		
		ID3D11ShaderResourceView* srv = nullptr;
		FAILED_ERROR_MESSAGE_ARGS(
			device->CreateShaderResourceView(dx11res, &rawBuffer->srvDescs[i].view, &srv),
			L"failed to create SRV(idx:%d)..",
			i
		);
		res->srvs[i] = srv;
	}
		
	ALLOC_RANGE_ZEROMEM(
		EASTL_PERSISTANT_NAME, res->uavCount, rawBuffer->uavDescs.size(),
		ID3D11UnorderedAccessView*, res->uavs
	);
	for (uint i = 0; i < res->uavCount; i++)
	{
		ID3D11Resource* dx11res = res->buffers[rawBuffer->uavDescs[i].bufferIndex];

		FAILED_ERROR_MESSAGE_ARGS(
			device->CreateUnorderedAccessView(dx11res, &rawBuffer->uavDescs[i].view, &res->uavs[i]),
			L"failed to create UAV(idx:%d)..",
			i
		);
	}

	ALLOC_RANGE_ZEROMEM(
		EASTL_PERSISTANT_NAME, res->inputLayoutItemCount, rawBuffer->inputLayoutDescs.size(),
		ID3D11InputLayout*, res->inputLayoutItems
	);
	for (uint i = 0; i < res->inputLayoutItemCount; i++)
	{
		FAILED_ERROR_MESSAGE_ARGS(
			device->CreateInputLayout(
				res->vertexLayouts[rawBuffer->inputLayoutDescs[i].vertexLayoutChunkIndex].descs,
				res->vertexLayouts[rawBuffer->inputLayoutDescs[i].vertexLayoutChunkIndex].descCount,
				res->shaders.vss[rawBuffer->inputLayoutDescs[i].vertexShaderIndex].shaderBlob->GetBufferPointer(),
				res->shaders.vss[rawBuffer->inputLayoutDescs[i].vertexShaderIndex].shaderBlob->GetBufferSize(),
				res->inputLayoutItems + i
			),
			L"failed to create input layout(idx:%d)..",
			i
		)
	}

	return S_OK;
}

HRESULT ReleaseResources(DX11Resources* res)
{
	SAFE_DELETE_OVERLOADED(res->geometryChunks, EASTL_PERSISTANT_NAME);
	SAFE_DELETE_OVERLOADED(res->shaderTex2Ds, EASTL_PERSISTANT_NAME);

	if (res->anims)
	{
		for (uint i = 0; i < res->animCount; i++)
			SAFE_DELETE_OVERLOADED(res->anims[i].animName, EASTL_PERSISTANT_NAME);
		SAFE_DELETE_OVERLOADED(res->anims, EASTL_PERSISTANT_NAME);
	}

	if (res->boneSets)
	{
		for (uint i = 0; i < res->boneSetCount; i++)
		{
			if (res->boneSets[i].bones)
			{
				for (uint j = 0; j < res->boneSets[i].boneCount; j++)
					SAFE_DELETE_OVERLOADED(res->boneSets[i].bones[j].name, EASTL_PERSISTANT_NAME);
				SAFE_DELETE_OVERLOADED(res->boneSets[i].bones, EASTL_PERSISTANT_NAME);
			}
		}
		SAFE_DELETE_OVERLOADED(res->boneSets, EASTL_PERSISTANT_NAME);
	}

	if (res->shaderFiles)
	{
		for (uint i = 0; i < res->shaderFileCount; i++)
		{
			DX11Resources::ShaderFile* file = res->shaderFiles + i;
			for (uint j = 0; j < 6; j++)
				SAFE_DELETE_OVERLOADED(file->shaderIndices[j].indices, EASTL_PERSISTANT_NAME);
		}
		SAFE_DELETE_OVERLOADED(res->shaderFiles, EASTL_PERSISTANT_NAME);
	}

	SAFE_DELETE_OVERLOADED(res->constantBufferIndices, EASTL_PERSISTANT_NAME);
	SAFE_DELETE_OVERLOADED(res->inputLayouts, EASTL_PERSISTANT_NAME);

	if (res->vertexLayouts)
	{
		for (uint i = 0; i < res->vertexLayoutCount; i++)
			SAFE_DELETE_OVERLOADED(res->vertexLayouts[i].descs, EASTL_PERSISTANT_NAME);
		SAFE_DELETE_OVERLOADED(res->vertexLayouts, EASTL_PERSISTANT_NAME);
	}

	if (res->samplerStates)
	{
		for (uint i = 0; i < res->samplerCount; i++)
			SAFE_RELEASE(res->samplerStates[i]);
		SAFE_DELETE_OVERLOADED(res->samplerStates, EASTL_PERSISTANT_NAME);
	}

	if (res->texture2Ds)
	{
		for (uint i = 0; i < res->texture2DCount; i++)
			SAFE_RELEASE(res->texture2Ds[i]);
		SAFE_DELETE_OVERLOADED(res->texture2Ds, EASTL_PERSISTANT_NAME);
	}

	if (res->buffers)
	{
		for (uint i = 0; i < res->bufferCount; i++)
			SAFE_RELEASE(res->buffers[i]);
		SAFE_DELETE_OVERLOADED(res->buffers, EASTL_PERSISTANT_NAME);
	}

	if (res->srvs)
	{
		for (uint i = 0; i < res->srvCount; i++)
			SAFE_RELEASE(res->srvs[i]);
		SAFE_DELETE_OVERLOADED(res->srvs, EASTL_PERSISTANT_NAME);
	}

	if (res->uavs)
	{
		for (uint i = 0; i < res->uavCount; i++)
			SAFE_RELEASE(res->uavs[i]);
		SAFE_DELETE_OVERLOADED(res->uavs, EASTL_PERSISTANT_NAME);
	}

	if (res->inputLayoutItems)
	{
		for (uint i = 0; i < res->inputLayoutItemCount; i++)
			SAFE_RELEASE(res->inputLayoutItems[i]);
		SAFE_DELETE_OVERLOADED(res->inputLayoutItems, EASTL_PERSISTANT_NAME);
	}

	for (uint i = 0; i < 6; i++)
	{
		for (uint j = 0; j < res->shadersByKind[i].shaderCount; j++)
		{
			SAFE_RELEASE(res->shadersByKind[i].shaders[j].shaderBlob);
			SAFE_RELEASE(res->shadersByKind[i].shaders[j].vs);
		}
		SAFE_DELETE_OVERLOADED(res->shadersByKind[i].shaders, EASTL_PERSISTANT_NAME);
	}

	return S_OK;
}

HRESULT LoadDX11Resoureces(DX11Resources* res, DX11InternalResourceDescBuffer* rawBuffer, DX11ResourceDesc* desc, ID3D11Device* device)
{
	HRESULT hr = S_OK;

	hr = LoadMeshAndAnimsFromFBXByDX11(res, rawBuffer, desc->fbxChunkCount, desc->fbxChunks, desc->fbxMeshConfigs);
	FAILED_ERROR_MESSAGE_RETURN(hr, L"fail to load geometries by FBX..");

	hr = ReserveTex2DAndSRVFromFileByDX11(
		res, rawBuffer, desc->textureDirCount, desc->texturedirs
	);
	FAILED_ERROR_MESSAGE_RETURN(hr, L"fail to load textures..");

	ALLOC_OVERLOADED_DEFINED(EASTL_TEMPARARY_NAME, dtosBuffer, DX11CompileDescToShader, desc->shaderCompileCount);

	hr = ReserveShaderFromFileByDX11(
		res, rawBuffer,
		desc->shaderCompileCount, desc->shaderCompileDescs, dtosBuffer
	);
	FAILED_ERROR_MESSAGE_RETURN(hr, L"fail to compile shaders..");

	hr = ReserveSkinningInstances(
		res, rawBuffer, desc->skinningInstanceCount, desc->skinningInstances
	);
	FAILED_ERROR_MESSAGE_RETURN(hr, L"fail to create dx11 input layout..");

	hr = ReserveLoadInputLayoutRefIndex(
		res, rawBuffer,
		desc->shaderCompileCount, dtosBuffer, desc->inputLayoutCount, desc->inputLayoutDescs
	);
	FAILED_ERROR_MESSAGE_RETURN(hr, L"fail to create dx11 input layout..");

	ALLOC_RANGE_ZEROMEM(
		EASTL_PERSISTANT_NAME, res->constantBufferCount, desc->constantBufferCount,
		uint, res->constantBufferIndices
	);
	uint count = ReserveLoadConstantBuffers(rawBuffer, desc->constantBufferCount, desc->constantBufferSizes);
	for (uint i = 0; i < desc->constantBufferCount; i++)
		res->constantBufferIndices[i] = count + i;

	count = ReserveLoadSamplerStates(rawBuffer, desc->samplerCount, desc->samplerDescs);

	size_t maxRawBufferSize = GetMaximumBufferSize(rawBuffer);
	void* buffer;// = (void*)new (EASTL_TEMPARARY_NAME, __FILE__, __LINE__) char[maxRawBufferSize];
	ALLOC_OVERLOADED_VOID_SIZED(EASTL_TEMPARARY_NAME, buffer, maxRawBufferSize);
	FAILED_ERROR_MESSAGE_RETURN(
		CreateDX11ResourcesByDesc(res, rawBuffer, buffer, device, true),
		L"fail to create dx11 samplers.."
	);
	SAFE_DELETE_OVERLOADED(buffer, EASTL_TEMPARARY_NAME);
	SAFE_DELETE_OVERLOADED(dtosBuffer, EASTL_TEMPARARY_NAME);

	return hr;
}

const char g_PositionSemanticName[] = "POSITION";
const char g_NormalSemanticName[] = "NORMAL";
const char g_TangentSemanticName[] = "TANGENT";
const char g_BinormalSemanticName[] = "BINORMAL";
const char g_UVSemanticName[] = "TEXCOORD";
const char g_BoneIndicesSemanticName[] = "BONEIDNCIES";
const char g_BoneWeightsSemanticName[] = "BONEWEIGHTS";

int BitSizeOfFormatElement(DXGI_FORMAT format);
int ByteSizeOfFormatElement(DXGI_FORMAT format);
bool EqualInputElementDesc(int descCount, D3D11_INPUT_ELEMENT_DESC* descArray0, D3D11_INPUT_ELEMENT_DESC* descArray1);

void SetDX11InputDescWithChunk(bool includeBone, int* descCounts, int* vertexSize/*[2]*/, D3D11_INPUT_ELEMENT_DESC* descBuffer, const FBXMeshChunk::FBXGeometryChunk* c)
{
	uint realAlignment = 0, vertexDataSize = 0;
	vertexSize[0] = 0;
	vertexSize[1] = 0;
	descCounts[0] = 0;

	descBuffer[descCounts[0]].SemanticName = g_PositionSemanticName;
	descBuffer[descCounts[0]].SemanticIndex = 0;
	descBuffer[descCounts[0]].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	descBuffer[descCounts[0]].InputSlot = 0;
	descBuffer[descCounts[0]].AlignedByteOffset = realAlignment;
	descBuffer[descCounts[0]].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	descBuffer[descCounts[0]].InstanceDataStepRate = 0;

	realAlignment += ByteSizeOfFormatElement(descBuffer[descCounts[0]].Format);
	descCounts[0]++;

	if (c->normals)
	{
		descBuffer[descCounts[0]].SemanticName = g_NormalSemanticName;
		descBuffer[descCounts[0]].SemanticIndex = 0;
		descBuffer[descCounts[0]].Format = DXGI_FORMAT_R32G32B32_FLOAT;
		descBuffer[descCounts[0]].InputSlot = 0;
		descBuffer[descCounts[0]].AlignedByteOffset = realAlignment;
		descBuffer[descCounts[0]].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
		descBuffer[descCounts[0]].InstanceDataStepRate = 0;

		realAlignment += ByteSizeOfFormatElement(descBuffer[descCounts[0]].Format);
		descCounts[0]++;
	}
	if (c->tangents)
	{
		descBuffer[descCounts[0]].SemanticName = g_TangentSemanticName;
		descBuffer[descCounts[0]].SemanticIndex = 0;
		descBuffer[descCounts[0]].Format = DXGI_FORMAT_R32G32B32_FLOAT;
		descBuffer[descCounts[0]].InputSlot = 0;
		descBuffer[descCounts[0]].AlignedByteOffset = realAlignment;
		descBuffer[descCounts[0]].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
		descBuffer[descCounts[0]].InstanceDataStepRate = 0;

		realAlignment += ByteSizeOfFormatElement(descBuffer[descCounts[0]].Format);
		descCounts[0]++;
	}
	if (c->binormals)
	{
		descBuffer[descCounts[0]].SemanticName = g_BinormalSemanticName;
		descBuffer[descCounts[0]].SemanticIndex = 0;
		descBuffer[descCounts[0]].Format = DXGI_FORMAT_R32G32B32_FLOAT;
		descBuffer[descCounts[0]].InputSlot = 0;
		descBuffer[descCounts[0]].AlignedByteOffset = realAlignment;
		descBuffer[descCounts[0]].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
		descBuffer[descCounts[0]].InstanceDataStepRate = 0;

		realAlignment += ByteSizeOfFormatElement(descBuffer[descCounts[0]].Format);
		descCounts[0]++;
	}
	for (uint uvi = 0; uvi < c->uvSlotCount; uvi++)
	{
		if (c->uvSlots[uvi])
		{
			descBuffer[descCounts[0]].SemanticName = g_UVSemanticName;
			descBuffer[descCounts[0]].SemanticIndex = uvi;
			descBuffer[descCounts[0]].Format = DXGI_FORMAT_R32G32_FLOAT;
			descBuffer[descCounts[0]].InputSlot = 0;
			descBuffer[descCounts[0]].AlignedByteOffset = realAlignment;
			descBuffer[descCounts[0]].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			descBuffer[descCounts[0]].InstanceDataStepRate = 0;

			realAlignment += ByteSizeOfFormatElement(descBuffer[descCounts[0]].Format);
			descCounts[0]++;
		}
	}
	if (includeBone && (c->boneIndices && c->boneWeights))
	{
		//descBuffer[descCounts[0]].SemanticName = g_BoneIndicesSemanticName;
		//descBuffer[descCounts[0]].SemanticIndex = 0;
		//descBuffer[descCounts[0]].Format = DXGI_FORMAT_R32G32B32A32_UINT;
		//descBuffer[descCounts[0]].InputSlot = 0;
		//descBuffer[descCounts[0]].AlignedByteOffset = realAlignment;
		//descBuffer[descCounts[0]].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
		//descBuffer[descCounts[0]].InstanceDataStepRate = 0;

		vertexDataSize += ByteSizeOfFormatElement(DXGI_FORMAT_R32G32B32A32_UINT);
		//descCounts[0]++;

		//descBuffer[descCounts[0]].SemanticName = g_BoneWeightsSemanticName;
		//descBuffer[descCounts[0]].SemanticIndex = 0;
		//descBuffer[descCounts[0]].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		//descBuffer[descCounts[0]].InputSlot = 0;
		//descBuffer[descCounts[0]].AlignedByteOffset = realAlignment;
		//descBuffer[descCounts[0]].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
		//descBuffer[descCounts[0]].InstanceDataStepRate = 0;

		vertexDataSize += ByteSizeOfFormatElement(DXGI_FORMAT_R32G32B32A32_FLOAT);
		//descCounts[0]++;
	}

	vertexSize[0] = realAlignment + vertexDataSize;
	vertexSize[1] = realAlignment;
}

int FindEqualDescIndex(uint descCount, D3D11_INPUT_ELEMENT_DESC* descBuffer, uint vertexLayoutBufferCount, DX11Resources::DX11LayoutChunk* vertexLayoutBuffer, DX11Resources* res)
{
	for (int vli = 0; vli < (int)res->vertexLayoutCount; vli++)
		if (
			res->vertexLayouts[vli].descCount == descCount &&
			EqualInputElementDesc(descCount, descBuffer, res->vertexLayouts[vli].descs)
			)
			return vli;


	for (int vli = 0; vli < (int)vertexLayoutBufferCount; vli++)
		if (
			vertexLayoutBuffer[vli].descCount == descCount &&
			EqualInputElementDesc(descCount, descBuffer, vertexLayoutBuffer[vli].descs)
			)
			return vli + res->vertexLayoutCount;

	return -1;
}

#include <iostream>

HRESULT LoadMeshAndAnimsFromFBXByDX11(
	DX11Resources* res, DX11InternalResourceDescBuffer* rawBuffer,
	uint chunkCount, const FBXChunk* chunks, const FBXChunkConfig* configs
)
{
	int startLayoutCount = res->vertexLayoutCount,
		newGeometryCount = 0, newBufferCount = 0, newSRVUAVCount = 0;
	for (uint ci = 0; ci < chunkCount; ci++)
		for (uint mi = 0; mi < chunks[ci].meshCount; mi++)
		{
			if (configs[ci].meshConfigs[mi].isSkinned)
			{
				newBufferCount += 3;
				newSRVUAVCount++;
			}
			else
				newBufferCount += 2;

			newGeometryCount++;
		}

	ALLOC_RANGE_ZEROMEM(
		EASTL_PERSISTANT_NAME, res->geometryCount, newGeometryCount,
		DX11Resources::GeometryChunk, res->geometryChunks
	);

	int vertexLayoutBufferCount = 0;
	DX11Resources::DX11LayoutChunk* vertexLayoutBuffer =
		(DX11Resources::DX11LayoutChunk*)alloca(
			sizeof(DX11Resources::DX11LayoutChunk) * res->geometryCount
		);

	const int descBufferCapacity = 32;
	D3D11_INPUT_ELEMENT_DESC* descBuffer = (D3D11_INPUT_ELEMENT_DESC*)alloca(sizeof(D3D11_INPUT_ELEMENT_DESC) * descBufferCapacity);
	memset(descBuffer, 0, sizeof(D3D11_INPUT_ELEMENT_DESC) * descBufferCapacity);

	uint totalAnimationCount = 0, totalBoneSetCount = 0;
	for (uint ci = 0; ci < chunkCount; ci++)
	{
		totalAnimationCount += chunks[ci].animationCount;
		totalBoneSetCount++;
	}

	ALLOC_RANGE_ZEROMEM(
		EASTL_PERSISTANT_NAME, res->boneSetCapacity, totalBoneSetCount,
		DX11Resources::BoneSet, res->boneSets
	);
	ALLOC_RANGE_ZEROMEM(
		EASTL_PERSISTANT_NAME, res->animCount, totalAnimationCount,
		DX11Resources::Animation, res->anims
	);

	for (
		uint ci = 0, geometryOffset = 0, bufferOffset = 0, uavOffset = 0, srvOffset = 0, animOffset = 0;
		ci < chunkCount;
		ci++, memset(descBuffer, 0, sizeof(D3D11_INPUT_ELEMENT_DESC) * descBufferCapacity)
		)
	{
		const FBXChunk& c = chunks[ci];

#pragma region load meshes from FBX
		for (uint mi = 0; mi < c.meshCount; mi++, memset(descBuffer, 0, sizeof(D3D11_INPUT_ELEMENT_DESC) * descBufferCapacity))
		{
			FBXChunkConfig::FBXMeshConfig& mc = configs[ci].meshConfigs[mi];
			FBXMeshChunk& m = c.meshs[mi];
			DX11Resources::GeometryChunk& g = res->geometryChunks[geometryOffset];
			g.bound = m.geometry.bound;

			// vertexlayout record start 

			int descCount, vertexSizes[2];
			SetDX11InputDescWithChunk(mc.isSkinned, &descCount, vertexSizes, descBuffer, &m.geometry);
			int vertexSize = mc.isSkinned ? vertexSizes[1] : vertexSizes[0];

			int findVertexLayoutIndex = FindEqualDescIndex(descCount, descBuffer, vertexLayoutBufferCount, vertexLayoutBuffer, res);

			if (findVertexLayoutIndex < 0)
			{
				vertexLayoutBuffer[vertexLayoutBufferCount].vertexSize = vertexSize;
				vertexLayoutBuffer[vertexLayoutBufferCount].descCount = descCount;
				ALLOC_OVERLOADED(
					EASTL_PERSISTANT_NAME, vertexLayoutBuffer[vertexLayoutBufferCount].descs, D3D11_INPUT_ELEMENT_DESC, descCount
				);
				memcpy(
					vertexLayoutBuffer[vertexLayoutBufferCount].descs,
					descBuffer,
					sizeof(D3D11_INPUT_ELEMENT_DESC) * descCount
				);
				findVertexLayoutIndex = vertexLayoutBufferCount;
				vertexLayoutBufferCount++;
			}
			// vertexlayout record end / remain layout index for geomtry buffer

			// geometry create buffer start
			g.vertexLayoutIndex = findVertexLayoutIndex;
			g.indexCount = m.geometry.indexCount;
			g.vertexCount = m.geometry.vertexCount;
			g.isSkinned = configs[ci].meshConfigs[mi].isSkinned;

			auto vertexCopy = [=](void* vptr) -> void
			{
				const FBXMeshChunk::FBXGeometryChunk& g = m.geometry;
				byte* ptr = static_cast<byte*>(vptr);
				for (uint i = 0; i < g.vertexCount; i++)
				{
					memcpy(ptr, g.vertices + i, sizeof(g.vertices[i]));
					ptr += sizeof(g.vertices[i]);
					if (g.normals)
					{
						memcpy(ptr, g.normals + i, sizeof(g.normals[i]));
						ptr += sizeof(g.normals[i]);
					}
					if (g.tangents)
					{
						memcpy(ptr, g.tangents + i, sizeof(g.tangents[i]));
						ptr += sizeof(g.tangents[i]);
					}
					if (g.binormals)
					{
						memcpy(ptr, g.binormals + i, sizeof(g.binormals[i]));
						ptr += sizeof(g.binormals[i]);
					}
					for (uint j = 0; j < g.uvSlotCount; j++)
						if (g.uvSlots[j])
						{
							memcpy(ptr, g.uvSlots[j] + i, sizeof(g.uvSlots[j][i]));
							ptr += sizeof(g.uvSlots[j][i]);
						}
					if (configs[ci].meshConfigs[mi].isSkinned && g.boneIndices && g.boneWeights)
					{
						memcpy(ptr, g.boneIndices + i, sizeof(g.boneIndices[i]));
						ptr += sizeof(g.boneIndices[i]);
						memcpy(ptr, g.boneWeights + i, sizeof(g.boneWeights[i]));
						ptr += sizeof(g.boneWeights[i]);
					}
				}
			};

			if (!configs[ci].meshConfigs[mi].isSkinned)
			{
				{
					DX11BufferDesc desc;
					memset(&desc, 0, sizeof(DX11BufferDesc));
					desc.buffer.Usage = D3D11_USAGE_DEFAULT;
					desc.buffer.ByteWidth = vertexSizes[0] * g.vertexCount;
					desc.buffer.BindFlags = D3D11_BIND_VERTEX_BUFFER;
					desc.buffer.CPUAccessFlags = 0;
					desc.copyToPtr = vertexCopy;

					g.vertexBufferIndex = ReserveLoadBuffer(rawBuffer, &desc);
				}
				{
					DX11BufferDesc desc;
					memset(&desc, 0, sizeof(DX11BufferDesc));
					desc.buffer.Usage = D3D11_USAGE_DEFAULT;
					desc.buffer.ByteWidth = sizeof(m.geometry.indices[0]) * m.geometry.indexCount;
					desc.buffer.BindFlags = D3D11_BIND_INDEX_BUFFER;
					desc.buffer.CPUAccessFlags = 0;
					desc.subres.pSysMem = m.geometry.indices;

					g.indexBufferIndex = ReserveLoadBuffer(rawBuffer, &desc);
				}
			}
			else
			{
				g.streamedVertexSize = vertexSizes[1];

				{
					DX11BufferDesc desc;
					desc.copyToPtr = vertexCopy;
					std::cout << "" << std::endl;

					desc.copyToPtr = nullptr;
				}

				{
					DX11BufferDesc desc;
					desc.buffer.Usage = D3D11_USAGE_DEFAULT;
					desc.buffer.ByteWidth = vertexSizes[0] * g.vertexCount;
					desc.buffer.BindFlags = D3D11_BIND_SHADER_RESOURCE;
					desc.buffer.CPUAccessFlags = 0;
					desc.buffer.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
					desc.buffer.StructureByteStride = vertexSizes[0];
					desc.copyToPtr = vertexCopy;

					g.vertexDataBufferIndex = ReserveLoadBuffer(rawBuffer, &desc);
				}
				{
					DX11BufferDesc desc;
					desc.buffer.Usage = D3D11_USAGE_DEFAULT;
					desc.buffer.ByteWidth = sizeof(m.geometry.indices[0]) * m.geometry.indexCount;
					desc.buffer.BindFlags = D3D11_BIND_INDEX_BUFFER;
					desc.buffer.CPUAccessFlags = 0;
					desc.subres.pSysMem = m.geometry.indices;
					g.indexBufferIndex = ReserveLoadBuffer(rawBuffer, &desc);
				}
				{
					DX11SRVDesc desc;
					desc.bufferIndex = g.vertexDataBufferIndex;
					desc.view.Format = DXGI_FORMAT_UNKNOWN;
					desc.view.ViewDimension = D3D11_SRV_DIMENSION::D3D11_SRV_DIMENSION_BUFFER;
					desc.view.Buffer.FirstElement = 0;
					desc.view.Buffer.NumElements = g.vertexCount;

					g.vertexDataSRVIndex = ReserveLoadShaderResourceView(rawBuffer, &desc);
				}
			}

			// geometry create end

			geometryOffset++;
		}

#pragma endregion
#pragma region load animations from fBX

		int findBoneSetIndex = -1;
		for (uint i = 0; i < res->boneSetCount; i++)
		{
			DX11Resources::BoneSet& boneSet = res->boneSets[i];

			if (boneSet.boneCount == c.hierarchyCount)
			{
				bool equal = true;
				for (uint j = 0; j < boneSet.boneCount; j++)
				{
					if (boneSet.bones[j].parentIndex != c.hierarchyNodes[j].parentIndex ||
						boneSet.bones[j].childCount != c.hierarchyNodes[j].childCount ||
						boneSet.bones[j].childIndexStart != c.hierarchyNodes[j].childIndexStart ||
						strcmp(boneSet.bones[j].name, c.hierarchyNodes[j].name) != 0 ||
						memcmp(
							&boneSet.bones[j].inverseGlobalTransformMatrix,
							&c.hierarchyNodes[j].inverseGlobalTransformMatrix,
							sizeof(boneSet.bones[j].inverseGlobalTransformMatrix)
						) != 0
						)
					{
						equal = false;
						break;
					}
				}

				if (equal)
				{
					findBoneSetIndex = i;
					break;
				}
			}
		}


		if (findBoneSetIndex < 0)
		{
			res->boneSetCount++;
			DX11Resources::BoneSet& boneSet = res->boneSets[res->boneSetCount - 1];

			ALLOC_RANGE_ZEROMEM(
				EASTL_PERSISTANT_NAME, boneSet.boneCount, c.hierarchyCount,
				DX11Resources::BoneSet::Bone, boneSet.bones
			);

			for (uint i = 0; i < c.hierarchyCount; i++)
				boneSet.bones[i].inverseGlobalTransformMatrix =
				c.hierarchyNodes[i].inverseGlobalTransformMatrix;

			DX11BufferDesc bd;
			memset(&bd, 0, sizeof(DX11BufferDesc));

			bd.buffer.Usage = D3D11_USAGE_DEFAULT;
			bd.buffer.ByteWidth = sizeof(Matrix4x4) * c.hierarchyCount;
			bd.buffer.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			bd.buffer.CPUAccessFlags = 0;
			bd.buffer.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
			bd.buffer.StructureByteStride = sizeof(Matrix4x4);
			bd.copyToPtr = [=](void* ptr) {
				Matrix4x4* matrixBuffer = static_cast<Matrix4x4*>(ptr);
				for (uint i = 0; i < boneSet.boneCount; i++)
					matrixBuffer[i] = boneSet.bones[i].inverseGlobalTransformMatrix;
			};
			res->boneSets[res->boneSetCount - 1].bindPoseTransformBufferIndex =
				ReserveLoadBuffer(rawBuffer, &bd);

			DX11SRVDesc srvd;
			memset(&srvd, 0, sizeof(DX11SRVDesc));

			srvd.bufferIndex = res->boneSets[res->boneSetCount - 1].bindPoseTransformBufferIndex;
			srvd.view.Format = DXGI_FORMAT_UNKNOWN;
			srvd.view.ViewDimension = D3D11_SRV_DIMENSION::D3D11_SRV_DIMENSION_BUFFER;
			srvd.view.Buffer.FirstElement = 0;
			srvd.view.Buffer.NumElements = c.hierarchyCount;

			res->boneSets[res->boneSetCount - 1].binePoseTransformSRVIndex =
				ReserveLoadShaderResourceView(rawBuffer, &srvd);

			findBoneSetIndex = res->boneSetCount - 1;
		}

		for (uint ai = 0; ai < c.animationCount; ai++)
		{
			DX11Resources::Animation& anim = res->anims[ai + animOffset];
			FBXChunk::FBXAnimation& fbxAnim = c.animations[ai];

			ALLOC_AND_STRCPY(EASTL_PERSISTANT_NAME, anim.animName, fbxAnim.animationName);
			anim.fpsCount = fbxAnim.fpsCount;
			anim.frameKeyCount = fbxAnim.frameKeyCount;
			anim.boneSetIndex = findBoneSetIndex;

			DX11BufferDesc bd;
			memset(&bd, 0, sizeof(DX11BufferDesc));

			bd.buffer.Usage = D3D11_USAGE_DEFAULT;
			bd.buffer.ByteWidth = sizeof(Matrix4x4) * fbxAnim.frameKeyCount * c.hierarchyCount;
			bd.buffer.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			bd.buffer.CPUAccessFlags = 0;
			bd.buffer.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
			bd.buffer.StructureByteStride = sizeof(Matrix4x4);
			bd.subres.pSysMem = fbxAnim.globalAffineTransforms;

			anim.animPoseTransformBufferIndex = ReserveLoadBuffer(rawBuffer, &bd);

			DX11SRVDesc srvd;
			memset(&srvd, 0, sizeof(DX11SRVDesc));

			srvd.bufferIndex = anim.animPoseTransformBufferIndex;
			srvd.view.Format = DXGI_FORMAT_UNKNOWN;
			srvd.view.ViewDimension = D3D11_SRV_DIMENSION::D3D11_SRV_DIMENSION_BUFFER;
			srvd.view.Buffer.FirstElement = 0;
			srvd.view.Buffer.NumElements = fbxAnim.frameKeyCount * c.hierarchyCount;

			anim.animPoseTransformSRVIndex = ReserveLoadShaderResourceView(rawBuffer, &srvd);

			animOffset++;
		}

#pragma endregion

	}

	// allocate & copy vertex layout
	res->vertexLayoutCount = vertexLayoutBufferCount;
	res->vertexLayouts =
		(DX11Resources::DX11LayoutChunk*)new (EASTL_PERSISTANT_NAME, __FILE__, __LINE__) 
		char[sizeof(DX11Resources::DX11LayoutChunk) * (res->vertexLayoutCount + res->vertexLayoutCount)];
	memcpy(
		res->vertexLayouts,
		vertexLayoutBuffer,
		sizeof(DX11Resources::DX11LayoutChunk) * res->vertexLayoutCount
	);

	return S_OK;
}

HRESULT ReserveTex2DAndSRVFromFileByDX11(
	DX11Resources* res, DX11InternalResourceDescBuffer* rawBuffer, uint dirCount, const wchar_t** dirs
)
{
	ASSERT(dirs != nullptr);

	ALLOC_RANGE_ZEROMEM(
		EASTL_PERSISTANT_NAME, res->shaderTex2DCount, dirCount,
		DX11Resources::ShaderTexture2D, res->shaderTex2Ds
	);

	for (uint i = 0; i < dirCount; i++)
	{
		DX11Resources::ShaderTexture2D& shaderTex2D = res->shaderTex2Ds[i];
		uint reservedTexture2DIndex = static_cast<uint>(rawBuffer->tex2DDescs.size());

		DX11SRVDesc srvd;
		memset(&srvd, 0, sizeof(srvd));
		srvd.texture2DIndex = reservedTexture2DIndex;
		srvd.view.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvd.view.Texture2D.MipLevels = 1;
		srvd.view.Texture2D.MostDetailedMip = 0;
		srvd.setSRVDesc = [=](D3D11_SHADER_RESOURCE_VIEW_DESC* viewDesc) -> bool
		{
			if (
				res->texture2Ds &&
				reservedTexture2DIndex < res->texture2DCount &&
				res->texture2Ds[reservedTexture2DIndex]
				)
			{
				D3D11_TEXTURE2D_DESC td;
				res->texture2Ds[reservedTexture2DIndex]->GetDesc(&td);
				viewDesc->Format = td.Format;
				return true;
			}
			else
				return false;
		};
		shaderTex2D.srvIndex = ReserveLoadShaderResourceView(rawBuffer, &srvd);

		DX11Texture2DDesc texDesc;
		memset(&texDesc, 0, sizeof(texDesc));
		texDesc.loadFromFile = true;
		texDesc.fileName = dirs[i];
		texDesc.usage = D3D11_USAGE::D3D11_USAGE_DEFAULT;
		texDesc.bindFlags = D3D11_BIND_SHADER_RESOURCE;
		texDesc.miscFlags = 0;
		texDesc.cpuAccessFlags = 0;
		texDesc.forceSRGB = false;

		shaderTex2D.tex2DIndex = ReserveLoadTexture2D(rawBuffer, &texDesc);
	}

	return S_OK;
}

HRESULT ReserveShaderFromFileByDX11(
	DX11Resources* res, DX11InternalResourceDescBuffer* rawBuffer,
	uint compileCount, const ShaderCompileDesc* descs, DX11CompileDescToShader* descToFileShader
)
{
	eastl::vector<DX11Resources::ShaderFile, EASTLAllocator> files = eastl::vector<DX11Resources::ShaderFile, EASTLAllocator>(EASTL_TEMPARARY_NAME);
	eastl::vector<eastl::array<eastl::vector<uint, EASTLAllocator>, 6>, EASTLAllocator> shaderIndicesByFile = 
		eastl::vector<eastl::array<eastl::vector<uint, EASTLAllocator>, 6>, EASTLAllocator>(EASTL_TEMPARARY_NAME);

	for (uint i = 0; i < compileCount; i++)
	{
		const ShaderCompileDesc& dsc = descs[i];
		int fileIndex = -1;
		for (int j = 0; j < files.size(); j++)
			if (wcscmp(files[j].fileName, dsc.fileName) == 0)
			{
				fileIndex = j;
				break;
			}

		if (fileIndex < 0)
		{
			DX11Resources::ShaderFile file;
			memset(&file, 0, sizeof(DX11Resources::ShaderFile));

			size_t len = wcslen(dsc.fileName);

			ALLOC_AND_WCSCPY(EASTL_PERSISTANT_NAME, file.fileName, dsc.fileName);

			fileIndex = (int)files.size();
			files.push_back(file);
			eastl::array<eastl::vector<uint, EASTLAllocator>, 6> arr(shaderIndicesByFile.push_back());
		}

		DX11Resources::ShaderFile& file = files[fileIndex];
		ShaderKind s;
		uint index = ReserveLoadShader(rawBuffer, &dsc, &s);
		if (index == UINT_MAX)
			continue;

		descToFileShader[i].shaderFileIndex = fileIndex;
		descToFileShader[i].shaderIndexInFile = static_cast<uint>(shaderIndicesByFile[fileIndex][(int)s].size());
		shaderIndicesByFile[fileIndex][(int)s].push_back(index);
		descToFileShader[i].shaderKindIndex = (uint)s;
		descToFileShader[i].shaderIndex = index;
	}

	ALLOC_RANGE_MEMCPY(
		EASTL_PERSISTANT_NAME, res->shaderFileCount, files.size(),
		DX11Resources::ShaderFile, res->shaderFiles, files.data()
	);

	for (uint i = 0; i < res->shaderFileCount; i++)
	{
		DX11Resources::ShaderFile& file = res->shaderFiles[i];

		for (int j = 0; j < 6; j++)
		{
			eastl::vector<uint, EASTLAllocator>& v = shaderIndicesByFile[i][j];
			file.shaderIndices[j].count = (uint)v.size();
			ALLOC_RANGE_ONLY_MEMCPY(
				EASTL_PERSISTANT_NAME, v.size(), uint, file.shaderIndices[j].indices, shaderIndicesByFile[i][(int)j].data()
			);
		}
	}

	return S_OK;
}

HRESULT ReserveSkinningInstances(
	DX11Resources* res, DX11InternalResourceDescBuffer* rawBuffer,
	uint skinningInstanceCount, const SkinningInstanceDesc* skinningInstances
)
{
	ALLOC_RANGE_ZEROMEM(
		EASTL_PERSISTANT_NAME, res->skinningCount, skinningInstanceCount,
		DX11Resources::SkinningInstance, res->skinningInstances
	);

	for (uint i = 0; i < res->skinningCount; i++)
	{
		auto& d = skinningInstances[i];
		auto& item = res->skinningInstances[i];
		auto& geometry = res->geometryChunks[d.geometryIndex];

		item.geometryIndex = d.geometryIndex;
		item.animationIndex = d.animationIndex;

		{
			DX11BufferDesc desc;
			memset(&desc, 0, sizeof(DX11BufferDesc));
			desc.buffer.Usage = D3D11_USAGE_DEFAULT;
			desc.buffer.ByteWidth = geometry.streamedVertexSize * geometry.vertexCount;
			desc.buffer.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
			desc.buffer.CPUAccessFlags = 0;
			desc.buffer.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
			desc.buffer.StructureByteStride = geometry.streamedVertexSize;

			item.vertexStreamBufferIndex = ReserveLoadBuffer(rawBuffer, &desc);
		}

		{
			//DX11BufferDesc desc;
			//memset(&desc, 0, sizeof(DX11BufferDesc));
			//desc.buffer.Usage = D3D11_USAGE_DEFAULT;
			//desc.buffer.ByteWidth = geometry.streamedVertexSize * geometry.vertexCount;
			//desc.buffer.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			//desc.buffer.CPUAccessFlags = 0;

			//item.vertexBufferIndex = ReserveLoadBuffer(rawBuffer, &desc);
		}

		{
			DX11SRVDesc desc;
			memset(&desc, 0, sizeof(DX11SRVDesc));

			desc.bufferIndex = item.vertexStreamBufferIndex;
			desc.view.Format = DXGI_FORMAT_UNKNOWN;
			desc.view.ViewDimension = D3D11_SRV_DIMENSION::D3D11_SRV_DIMENSION_BUFFER;
			desc.view.BufferEx.FirstElement = 0;
			desc.view.BufferEx.NumElements = geometry.vertexCount;
			desc.view.BufferEx.Flags = 0;

			item.vertexStreamSRVIndex = ReserveLoadShaderResourceView(rawBuffer, &desc);
		}
		{
			DX11UAVDesc desc;
			memset(&desc, 0, sizeof(DX11UAVDesc));

			desc.bufferIndex = item.vertexStreamBufferIndex;
			desc.view.Format = DXGI_FORMAT_UNKNOWN;
			desc.view.ViewDimension = D3D11_UAV_DIMENSION::D3D11_UAV_DIMENSION_BUFFER;
			desc.view.Buffer.FirstElement = 0;
			desc.view.Buffer.NumElements = geometry.vertexCount;
			desc.view.Buffer.Flags = 0;

			item.vertexStreamUAVIndex = ReserveLoadUnorderedAccessView(rawBuffer, &desc);
		}
	}
	return S_OK;
}

HRESULT UploadDX11ConstantBuffer(
	DX11Resources* res, ID3D11DeviceContext* deviceContext, uint constantBufferIndex, void* uploadData
)
{
	HRESULT hr = S_OK;
	deviceContext->UpdateSubresource(res->buffers[res->constantBufferIndices[constantBufferIndex]], 0, nullptr, uploadData, 0, 0);
	return hr;
}
