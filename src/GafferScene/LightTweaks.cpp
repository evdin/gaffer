//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2016, Image Engine Design Inc. All rights reserved.
//
//  Redistribution and use in source and binary forms, with or without
//  modification, are permitted provided that the following conditions are
//  met:
//
//      * Redistributions of source code must retain the above
//        copyright notice, this list of conditions and the following
//        disclaimer.
//
//      * Redistributions in binary form must reproduce the above
//        copyright notice, this list of conditions and the following
//        disclaimer in the documentation and/or other materials provided with
//        the distribution.
//
//      * Neither the name of John Haddon nor the names of
//        any other contributors to this software may be used to endorse or
//        promote products derived from this software without specific prior
//        written permission.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
//  IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
//  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
//  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
//  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
//  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
//  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
//  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
//  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
//  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
//  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//////////////////////////////////////////////////////////////////////////

#include "GafferScene/LightTweaks.h"

#include "GafferScene/TweakPlug.h"

#include "Gaffer/CompoundDataPlug.h"

#include "IECoreScene/Shader.h"
#include "IECoreScene/ShaderNetwork.h"

#include "IECore/SimpleTypedData.h"
#include "IECore/StringAlgo.h"

using namespace std;
using namespace IECore;
using namespace IECoreScene;
using namespace Gaffer;
using namespace GafferScene;

IE_CORE_DEFINERUNTIMETYPED( LightTweaks );

size_t LightTweaks::g_firstPlugIndex = 0;

LightTweaks::LightTweaks( const std::string &name )
	:	SceneElementProcessor( name, IECore::PathMatcher::NoMatch )
{
	storeIndexOfNextChild( g_firstPlugIndex );
	addChild( new StringPlug( "type", Plug::In, "light *:light" ) );
	addChild( new Plug( "tweaks" ) );

	// Fast pass-throughs for the things we don't alter.
	outPlug()->objectPlug()->setInput( inPlug()->objectPlug() );
	outPlug()->transformPlug()->setInput( inPlug()->transformPlug() );
	outPlug()->boundPlug()->setInput( inPlug()->boundPlug() );
}

LightTweaks::~LightTweaks()
{
}

Gaffer::StringPlug *LightTweaks::typePlug()
{
	return getChild<Gaffer::StringPlug>( g_firstPlugIndex );
}

const Gaffer::StringPlug *LightTweaks::typePlug() const
{
	return getChild<Gaffer::StringPlug>( g_firstPlugIndex );
}

Gaffer::Plug *LightTweaks::tweaksPlug()
{
	return getChild<Gaffer::Plug>( g_firstPlugIndex + 1 );
}

const Gaffer::Plug *LightTweaks::tweaksPlug() const
{
	return getChild<Gaffer::Plug>( g_firstPlugIndex + 1 );
}

void LightTweaks::affects( const Gaffer::Plug *input, AffectedPlugsContainer &outputs ) const
{
	SceneElementProcessor::affects( input, outputs );

	if( tweaksPlug()->isAncestorOf( input ) || input == typePlug() )
	{
		outputs.push_back( outPlug()->attributesPlug() );
	}
}

bool LightTweaks::processesAttributes() const
{
	// Although the base class says that we should return a constant, it should
	// be OK to return this because it's constant across the hierarchy.
	return !tweaksPlug()->children().empty();
}

void LightTweaks::hashProcessedAttributes( const ScenePath &path, const Gaffer::Context *context, IECore::MurmurHash &h ) const
{
	typePlug()->hash( h );
	for( TweakPlugIterator tIt( tweaksPlug() ); !tIt.done(); ++tIt )
	{
		for( ValuePlugIterator vIt( tIt->get() ); !vIt.done(); ++vIt )
		{
			(*vIt)->hash( h );
		}
	}
}

IECore::ConstCompoundObjectPtr LightTweaks::computeProcessedAttributes( const ScenePath &path, const Gaffer::Context *context, IECore::ConstCompoundObjectPtr inputAttributes ) const
{
	const string type = typePlug()->getValue();
	if( type.empty() )
	{
		return inputAttributes;
	}

	const Plug *tweaksPlug = this->tweaksPlug();
	if( tweaksPlug->children().empty() )
	{
		return inputAttributes;
	}

	CompoundObjectPtr result = new CompoundObject;
	const CompoundObject::ObjectMap &in = inputAttributes->members();
	CompoundObject::ObjectMap &out = result->members();
	for( CompoundObject::ObjectMap::const_iterator it = in.begin(), eIt = in.end(); it != eIt; ++it )
	{
		if( !StringAlgo::matchMultiple( it->first, type ) )
		{
			out.insert( *it );
			continue;
		}
		const ShaderNetwork *network = runTimeCast<const ShaderNetwork>( it->second.get() );
		if( !network )
		{
			out.insert( *it );
			continue;
		}

		ShaderNetworkPtr tweakedNetwork = network->copy();
		ShaderPtr tweakedShader = tweakedNetwork->outputShader()->copy();

		for( TweakPlugIterator tIt( tweaksPlug ); !tIt.done(); ++tIt )
		{
			(*tIt)->applyTweak( tweakedShader->parametersData(), true /* Require parameters to already exist */ );

			// Note that it doesn't make sense to allow adding new parameters to lights - lights start out with
			// all supported parameters defined
		}

		tweakedNetwork->setShader( tweakedNetwork->getOutput().shader, std::move( tweakedShader ) );
		out[it->first] = tweakedNetwork;
	}

	return result;
}
