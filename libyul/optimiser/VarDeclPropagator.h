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

#pragma once

#include <libyul/optimiser/ASTWalker.h>
#include <libyul/Exceptions.h>
#include <libsolidity/inlineasm/AsmData.h>
#include <list>
#include <stack>
#include <vector>
#include <set>

namespace dev
{
namespace yul
{

/**
 * Rewrites Assignment statements into VariableDeclaration when the assignment's LHS
 * variables had no value yet.
 *
 * It recursively walks through the AST.
 * For each Block statement, the VariableDeclaration statements without value are tracked.
 * For each Assignment statement it is checked if the LHS is first-hand initializing
 * the corresponding VariableDeclaration.
 * If so, the Assignment is rewritten into a VariableDeclaration and the original
 * VariableDeclaration statement(s) are marked for deletion.
 * At the end of every Block traversal, all pending deletions are performed and
 * the outer's scope working variables are restored.
 */
class VarDeclPropagator: public ASTModifier
{
public:
	using ASTModifier::operator();
	void operator()(Block& _block) override;
	void operator()(VariableDeclaration& _varDecl) override;
	void operator()(Assignment& _assignment) override;

private:
	using TypedNameList = solidity::assembly::TypedNameList;
	using TypedName = solidity::assembly::TypedName;
	using Statement = solidity::assembly::Statement;

	inline Block& currentBlock() { return *m_currentBlock; }

	bool checkAllVarDeclsEmpty(std::vector<Identifier> const& _varNames) const;
	TypedName const& getTypedName(Identifier const& _identifier) const;
	void markVarDeclForRemoval(std::vector<Identifier> const& _identifiers);
	bool isEmptyVarDecl(Identifier const& _identifier) const;
	TypedNameList recreateLvalueTypedNameList(Assignment& _assignment);

	template<typename StmtT> std::vector<Statement>::iterator iteratorOf(StmtT& stmt);

private:
	Block* m_currentBlock;
	std::list<VariableDeclaration*> m_emptyVarDecls;
	std::set<VariableDeclaration*> m_pendingForRemoval;
};

}
}
