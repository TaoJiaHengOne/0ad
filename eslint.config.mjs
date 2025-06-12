// NODE_PATH isn't supprorted for ESM modules [1], so for eslint to be able to
// be run via pre-commit use a workaround instead of static import.
// [1] https://nodejs.org/api/esm.html#esm_no_node_path
import { createRequire } from 'node:module';
const require = createRequire(import.meta.url);
const braceRules = require("eslint-plugin-brace-rules");
const stylistic = require("@stylistic/eslint-plugin");


const configIgnores = {
	"ignores": [
		"binaries/data/mods/mod/globalscripts/sprintf.js",
		"libraries/",
		"source/tools/profiler2/jquery*",
		"source/tools/replayprofile/jquery*",
		"source/tools/templatesanalyzer/tablefilter/",

		// This files deliberately contain errors
		"binaries/data/mods/_test.scriptinterface/module/import_inside_function.js",
		"binaries/data/mods/_test.scriptinterface/module/export_default/invalid.js",
	],
};


const configEslintBase = {
	"rules": {
		"no-caller": 1,
		"no-cond-assign": 1,

		"no-constant-condition": ["error", {
			"checkLoops": false,
		}],

		"no-dupe-args": 1,
		"no-dupe-keys": 1,
		"no-duplicate-case": 1,
		"no-empty": 1,
		"no-extra-boolean-cast": 0,
		"no-func-assign": 1,
		"no-unsafe-negation": ["warn", { "enforceForOrderingRelations": true }],
		"no-obj-calls": 1,
		"no-unreachable": 1,
		"no-use-before-define": ["error", "nofunc"],
		"use-isnan": 1,
		"valid-jsdoc": 0,
		"valid-typeof": 1,
		"block-scoped-var": 0,
		"consistent-return": 1,
		"default-case": 1,
		"dot-notation": 1,
		"no-else-return": 1,
		"no-invalid-this": 1,
		"no-loop-func": 0,

		"no-new": 1,
		"no-redeclare": 0,
		"no-return-assign": 1,
		"no-self-assign": 1,
		"no-self-compare": 1,
		"no-unmodified-loop-condition": 1,
		"no-unused-expressions": 1,
		"no-unused-labels": 1,
		"no-useless-concat": 0,
		"yoda": 1,
		"no-delete-var": 1,
		"no-label-var": 1,
		"no-shadow-restricted-names": 1,
		"no-shadow": 1,
		"no-undef": 0,
		"no-undef-init": 1,
		"no-unused-vars": 0,


		"new-cap": 0,
		"no-multi-assign": 1,
		"no-unneeded-ternary": 1,
		"no-irregular-whitespace": 1,
		"operator-assignment": 1,
		"no-class-assign": 1,
		"no-const-assign": 1,
		"no-dupe-class-members": 1,
		"prefer-const": 1,
	}
};


const configStylistic = {
	"plugins": {
		'@stylistic': stylistic
	},

	"rules": {
		"@stylistic/comma-spacing": "warn",
		"@stylistic/indent": ["warn", "tab", { "outerIIFEBody": "off" }],
		"@stylistic/key-spacing": "warn",
		"@stylistic/new-parens": "warn",
		"@stylistic/no-extra-parens": "off",
		"@stylistic/no-extra-semi": "warn",
		"@stylistic/no-floating-decimal": "warn",
		"@stylistic/no-mixed-spaces-and-tabs": ["warn", "smart-tabs"],
		"@stylistic/no-multi-spaces": ["warn", { "ignoreEOLComments": true }],
		"@stylistic/no-trailing-spaces": "warn",
		"@stylistic/object-curly-spacing": ["warn", "always"],
		"@stylistic/operator-linebreak": ["warn", "after"],
		"@stylistic/quote-props": "warn",
		"@stylistic/semi": "warn",
		"@stylistic/semi-spacing": "warn",
		"@stylistic/space-before-function-paren": ["warn", "never"],
		"@stylistic/space-in-parens": "warn",
		"@stylistic/space-unary-ops": "warn",
		"@stylistic/spaced-comment": ["warn", "always"],
	}
};


const configBracesRules = {
	"plugins": {
		"brace-rules": braceRules
	},

	"rules": {
		"brace-rules/brace-on-same-line": [
			"warn",
			{
				"FunctionDeclaration": "never",
				"FunctionExpression": "ignore",
				"ArrowFunctionExpression": "always",
				"IfStatement": "never",
				"TryStatement": "ignore",
				"CatchClause": "ignore",
				"DoWhileStatement": "never",
				"WhileStatement": "never",
				"ForStatement": "never",
				"ForInStatement": "never",
				"ForOfStatement": "never",
				"SwitchStatement": "never",
			},
			{
				"allowSingleLine": true,
			}
		],
	}
};


const configs = [configIgnores, configEslintBase];
configs[1].plugins = { ...configBracesRules.plugins };
Object.assign(configs[1].rules, configBracesRules.rules);
Object.assign(configs[1].plugins, configStylistic.plugins);
Object.assign(configs[1].rules, configStylistic.rules);

export default configs;
