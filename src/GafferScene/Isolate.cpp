//////////////////////////////////////////////////////////////////////////
//  
//  Copyright (c) 2013, Image Engine Design Inc. All rights reserved.
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

#include "Gaffer/Context.h"

#include "GafferScene/Isolate.h"
#include "GafferScene/PathMatcherData.h"

using namespace std;
using namespace IECore;
using namespace Gaffer;
using namespace GafferScene;

IE_CORE_DEFINERUNTIMETYPED( Isolate );

size_t Isolate::g_firstPlugIndex = 0;

Isolate::Isolate( const std::string &name )
	:	FilteredSceneProcessor( name, Filter::EveryMatch )
{
	storeIndexOfNextChild( g_firstPlugIndex );
	addChild( new BoolPlug( "adjustBounds", Plug::In, false ) );
}

Isolate::~Isolate()
{
}

Gaffer::BoolPlug *Isolate::adjustBoundsPlug()
{
	return getChild<BoolPlug>( g_firstPlugIndex );
}

const Gaffer::BoolPlug *Isolate::adjustBoundsPlug() const
{
	return getChild<BoolPlug>( g_firstPlugIndex );
}

void Isolate::affects( const Gaffer::Plug *input, AffectedPlugsContainer &outputs ) const
{
	FilteredSceneProcessor::affects( input, outputs );

	const ScenePlug *in = inPlug();
	if( input->parent<ScenePlug>() == in )
	{
		outputs.push_back( outPlug()->getChild<ValuePlug>( input->getName() ) );
	}
	else if( input == filterPlug() )
	{
		outputs.push_back( outPlug()->childNamesPlug() );
		outputs.push_back( outPlug()->globalsPlug() );
	}
	else if( input == adjustBoundsPlug() )
	{
		outputs.push_back( outPlug()->boundPlug() );
	}
}

void Isolate::hashBound( const ScenePath &path, const Gaffer::Context *context, const ScenePlug *parent, IECore::MurmurHash &h ) const
{
	if( adjustBoundsPlug()->getValue() )
	{
		if( filterValue( context ) == Filter::DescendantMatch )
		{
			h = hashOfTransformedChildBounds( path, outPlug() );
			return;
		}
	}

	// pass through
	h = inPlug()->boundPlug()->hash();
}

void Isolate::hashTransform( const ScenePath &path, const Gaffer::Context *context, const ScenePlug *parent, IECore::MurmurHash &h ) const
{
	h = inPlug()->transformPlug()->hash();
}

void Isolate::hashAttributes( const ScenePath &path, const Gaffer::Context *context, const ScenePlug *parent, IECore::MurmurHash &h ) const
{
	h = inPlug()->attributesPlug()->hash();
}

void Isolate::hashObject( const ScenePath &path, const Gaffer::Context *context, const ScenePlug *parent, IECore::MurmurHash &h ) const
{
	h = inPlug()->objectPlug()->hash();
}

void Isolate::hashChildNames( const ScenePath &path, const Gaffer::Context *context, const ScenePlug *parent, IECore::MurmurHash &h ) const
{
	if( filterValue( context ) == Filter::DescendantMatch )
	{
		// we might be computing new childnames for this level.
		FilteredSceneProcessor::hashChildNames( path, context, parent, h );
		inPlug()->childNamesPlug()->hash( h );
		filterHash( context, h );
	}
	else
	{
		// pass through
		h = inPlug()->childNamesPlug()->hash();
	}
}

void Isolate::hashGlobals( const Gaffer::Context *context, const ScenePlug *parent, IECore::MurmurHash &h ) const
{
	FilteredSceneProcessor::hashGlobals( context, parent, h );
	inPlug()->globalsPlug()->hash( h );
	filterHash( context, h );
}

Imath::Box3f Isolate::computeBound( const ScenePath &path, const Gaffer::Context *context, const ScenePlug *parent ) const
{
	if( adjustBoundsPlug()->getValue() )
	{
		if( filterValue( context ) == Filter::DescendantMatch )
		{
			return unionOfTransformedChildBounds( path, outPlug() );
		}
	}
	
	return inPlug()->boundPlug()->getValue();
}

Imath::M44f Isolate::computeTransform( const ScenePath &path, const Gaffer::Context *context, const ScenePlug *parent ) const
{
	return inPlug()->transformPlug()->getValue();
}

IECore::ConstCompoundObjectPtr Isolate::computeAttributes( const ScenePath &path, const Gaffer::Context *context, const ScenePlug *parent ) const
{
	return inPlug()->attributesPlug()->getValue();
}

IECore::ConstObjectPtr Isolate::computeObject( const ScenePath &path, const Gaffer::Context *context, const ScenePlug *parent ) const
{
	return inPlug()->objectPlug()->getValue();
}

IECore::ConstInternedStringVectorDataPtr Isolate::computeChildNames( const ScenePath &path, const Gaffer::Context *context, const ScenePlug *parent ) const
{
	if( filterValue( context ) == Filter::DescendantMatch )
	{
		// we may need to delete one or more of our children
		ConstInternedStringVectorDataPtr inputChildNamesData = inPlug()->childNamesPlug()->getValue();
		const vector<InternedString> &inputChildNames = inputChildNamesData->readable();
		
		InternedStringVectorDataPtr outputChildNamesData = new InternedStringVectorData;
		vector<InternedString> &outputChildNames = outputChildNamesData->writable();
		
		ContextPtr tmpContext = new Context( *context, Context::Borrowed );
		Context::Scope scopedContext( tmpContext );

		ScenePath childPath = path;
		childPath.push_back( InternedString() ); // for the child name
		for( vector<InternedString>::const_iterator it = inputChildNames.begin(), eIt = inputChildNames.end(); it != eIt; it++ )
		{
			childPath[path.size()] = *it;
			tmpContext->set( ScenePlug::scenePathContextName, childPath );
			if( filterValue( tmpContext.get() ) != Filter::NoMatch )
			{
				outputChildNames.push_back( *it );
			}
		}
		
		return outputChildNamesData;
	}
	else
	{
		// pass through
		return inPlug()->childNamesPlug()->getValue();
	}
}

IECore::ConstCompoundObjectPtr Isolate::computeGlobals( const Gaffer::Context *context, const ScenePlug *parent ) const
{
	ConstCompoundObjectPtr inputGlobals = inPlug()->globalsPlug()->getValue();
	const CompoundData *inputSets = inputGlobals->member<CompoundData>( "gaffer:sets" );
	if( !inputSets )
	{
		return inputGlobals;
	}
	
	CompoundObjectPtr outputGlobals = inputGlobals->copy();
	CompoundDataPtr outputSets = new CompoundData;
	outputGlobals->members()["gaffer:sets"] = outputSets;

	ContextPtr tmpContext = new Context( *context, Context::Borrowed );
	Context::Scope scopedContext( tmpContext );
	ScenePath path;

	for( CompoundDataMap::const_iterator it = inputSets->readable().begin(), eIt = inputSets->readable().end(); it != eIt; ++it )
	{
		/// \todo This could be more efficient if PathMatcher exposed the internal nodes,
		/// and allowed sharing between matchers. Then we could do a really lightweight copy
		/// and just trim out the nodes we didn't want.
		const PathMatcher &inputSet = static_cast<const PathMatcherData *>( it->second.get() )->readable();
		PathMatcher &outputSet = outputSets->member<PathMatcherData>( it->first, /* throwExceptions = */ false, /* createIfMissing = */ true )->writable();
		
		vector<string> inputPaths;
		inputSet.paths( inputPaths );
		for( vector<string>::const_iterator pIt = inputPaths.begin(), peIt = inputPaths.end(); pIt != peIt; ++pIt )
		{
			path.clear();
			ScenePlug::stringToPath( *pIt, path );

			tmpContext->set( ScenePlug::scenePathContextName, path );
			if( filterValue( tmpContext ) != Filter::NoMatch )
			{
				outputSet.addPath( *pIt );
			}
		}
	}
	
	return outputGlobals;
}

