/**
 * @file stmt.h
 * @brief Statement parsing declarations for the Alkyl parser.
 */
#ifndef PARSER_STMT_H
#define PARSER_STMT_H

#include "parser_internal.h"

/**
 * @brief Parses a single statement or block.
 * @param p The parser.
 * @return The parsed statement AST node.
 */
ASTNode* parse_single_statement_or_block(Parser *p);

/**
 * @brief Parses a sequence of statements.
 * @param p The parser.
 * @return The head of the parsed statement list.
 */
ASTNode* parse_statements(Parser *p);

/**
 * @brief Parses a variable declaration.
 * @param p The parser.
 * @return The parsed variable declaration AST node.
 */
ASTNode* parse_var_decl_internal(Parser *p);

/**
 * @brief Parses an assignment or function call.
 * @param p The parser.
 * @return The parsed assignment or call AST node.
 */
ASTNode* parse_assignment_or_call(Parser *p);

/**
 * @brief Parses a loop statement.
 * @param p The parser.
 * @return The parsed loop AST node.
 */
ASTNode* parse_loop(Parser *p);

/**
 * @brief Parses a while statement.
 * @param p The parser.
 * @return The parsed while AST node.
 */
ASTNode* parse_while(Parser *p);

/**
 * @brief Parses an if statement.
 * @param p The parser.
 * @return The parsed if AST node.
 */
ASTNode* parse_if(Parser *p);

/**
 * @brief Parses a switch statement.
 * @param p The parser.
 * @return The parsed switch AST node.
 */
ASTNode* parse_switch(Parser *p); 

/**
 * @brief Parses a return statement.
 * @param p The parser.
 * @return The parsed return AST node.
 */
ASTNode* parse_return(Parser *p);

/**
 * @brief Parses a break statement.
 * @param p The parser.
 * @return The parsed break AST node.
 */
ASTNode* parse_break(Parser *p);

/**
 * @brief Parses a continue statement.
 * @param p The parser.
 * @return The parsed continue AST node.
 */
ASTNode* parse_continue(Parser *p);

/**
 * @brief Parses an emit statement.
 * @param p The parser.
 * @return The parsed emit AST node.
 */
ASTNode* parse_emit(Parser *p);

/**
 * @brief Parses a for-in statement.
 * @param p The parser.
 * @return The parsed for-in AST node.
 */
ASTNode* parse_for_in(Parser *p);

#endif // PARSER_STMT_H
