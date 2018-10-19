/*
	This file is part of solidity.

	solidity is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	solidity is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with solidity.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <libyul/optimiser/VarDeclPropagator.h>
#include <libsolidity/inlineasm/AsmData.h>
#include <libdevcore/Visitor.h>
#include <algorithm>
#include <set>

#include <boost/algorithm/cxx11/all_of.hpp>

using namespace std;
using namespace dev;
using namespace dev::yul;

using dev::solidity::assembly::TypedName;
using dev::solidity::assembly::TypedNameList;

// GRAND-GOAL: eliminate any empty VariableDeclaration statements
//
// CASE-match-exact
//    let x, y, z
//    z, x, y := RHS
// -->
//    let z, x, y := RHS
//
// CASE-single-let-scattered-assign
//     let x, y
//     x := RHS_1
//     y := RHS_2
// -->
//     let x := RHS_1
//     let y := RHS_2
//
// CASE-multi-let-single-assign
//     let x
//     let y
//     x, y := f()
// -->
//     let x, y := f()
//
// CASE-unused-var-component
//     let x, y
//     x := f()
//     (y never assigned to)
// -->
//     let x := f()
//
// CASE-mixed
//     let x := f()
//     let y
//     x, y := g()
// -->
//     ???

void VarDeclPropagator::operator()(Block& _block)
{
	std::list<VariableDeclaration*> emptyVarDecls;
	std::set<VariableDeclaration*> pendingForRemoval;

	swap(emptyVarDecls, m_emptyVarDecls);
	swap(pendingForRemoval, m_pendingForRemoval);
	Block* outerBlock = m_currentBlock;
	m_currentBlock = &_block;

	ASTModifier::operator()(_block);

	for (VariableDeclaration* varDecl : m_pendingForRemoval)
		_block.statements.erase(iteratorOf(*varDecl));

	m_pendingForRemoval = move(pendingForRemoval);
	m_emptyVarDecls = move(emptyVarDecls);
	m_currentBlock = outerBlock;
}

void VarDeclPropagator::operator()(VariableDeclaration& _varDecl)
{
	if (!_varDecl.value)
		m_emptyVarDecls.push_back(&_varDecl);
}

void VarDeclPropagator::operator()(Assignment& _assignment)
{
	yulAssert(!_assignment.variableNames.empty(), "LHS must not be empty");

	if (checkAllVarDeclsEmpty(_assignment.variableNames))
	{
		markVarDeclForRemoval(_assignment.variableNames);

		*iteratorOf(_assignment) = VariableDeclaration{
			_assignment.location,
			recreateLvalueTypedNameList(_assignment),
			_assignment.value
		};
	}
}

bool VarDeclPropagator::checkAllVarDeclsEmpty(std::vector<Identifier> const& _varNames) const
{
	for (Identifier const& ident : _varNames)
		if (!isEmptyVarDecl(ident))
			return false;

	return true;
}

bool VarDeclPropagator::isEmptyVarDecl(Identifier const& _identifier) const
{
	for (VariableDeclaration const* varDecl : m_emptyVarDecls)
		for (TypedName const& typedName : varDecl->variables)
			if (typedName.name == _identifier.name)
				return true;

	return false;
}

template<typename StmtT>
std::vector<Statement>::iterator VarDeclPropagator::iteratorOf(StmtT& _stmt)
{
	auto it = find_if(
		begin(currentBlock().statements),
		end(currentBlock().statements),
		[&](Statement& checkStmt) { return checkStmt.type() == typeid(StmtT)
											&& &boost::get<StmtT>(checkStmt) == &_stmt; }
	);
	yulAssert(it != end(currentBlock().statements), "");
	return it;
}

TypedNameList VarDeclPropagator::recreateLvalueTypedNameList(Assignment& _assignment)
{
	TypedNameList aux;
	aux.reserve(_assignment.variableNames.size());

	for (Identifier const& varName : _assignment.variableNames)
		aux.emplace_back(getTypedName(varName));

	return aux;
}

TypedName const& VarDeclPropagator::getTypedName(Identifier const& _identifier) const
{
	for (VariableDeclaration* varDecl : m_emptyVarDecls)
		for (TypedName const& typedName : varDecl->variables)
			if (typedName.name == _identifier.name)
				return typedName;

	yulAssert(false, "Unexpectly not found.");
}

void VarDeclPropagator::markVarDeclForRemoval(std::vector<Identifier> const& _identifiers)
{
	for (Identifier const& ident : _identifiers)
		for (VariableDeclaration* varDecl : m_emptyVarDecls)
			for (TypedName const& typedName : varDecl->variables)
				if (typedName.name == ident.name)
					m_pendingForRemoval.insert(varDecl);
}
