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


const configEslintRecommended = {
	"rules": {
		"no-class-assign": "warn",
		"no-cond-assign": "warn",
		"no-const-assign": "warn",
		"no-constant-condition": ["error", { "checkLoops": false }],
		"no-delete-var": "warn",
		"no-dupe-args": "warn",
		"no-dupe-class-members": "warn",
		"no-dupe-keys": "warn",
		"no-duplicate-case": "warn",
		"no-empty": "warn",
		"no-extra-boolean-cast": "warn",
		"no-func-assign": "warn",
		"no-irregular-whitespace": "warn",
		"no-obj-calls": "warn",
		"no-redeclare": "warn",
		"no-self-assign": "warn",
		"no-shadow-restricted-names": "warn",
		/* "no-undef": "warn", */
		"no-unreachable": "warn",
		"no-unsafe-negation": ["warn", { "enforceForOrderingRelations": true }],
		"no-unused-labels": "warn",
		"no-unused-vars": "off",
		"use-isnan": "warn",
		"valid-typeof": "warn",
	}
};


const configEslintExtra = {
	"rules": {
		"block-scoped-var": "off",
		"consistent-return": "warn",
		"default-case": "warn",
		"dot-notation": "warn",
		"new-cap": "off",
		"no-caller": "warn",
		"no-else-return": "warn",
		"no-invalid-this": "warn",
		"no-label-var": "warn",
		"no-loop-func": "off",
		"no-multi-assign": "warn",
		"no-new": "warn",
		"no-return-assign": "warn",
		"no-self-compare": "warn",
		"no-shadow": "warn",
		"no-undef-init": "warn",
		"no-unmodified-loop-condition": "warn",
		"no-unneeded-ternary": "warn",
		"no-unused-expressions": "warn",
		"no-use-before-define": ["error", "nofunc"],
		"no-useless-concat": "off",
		"operator-assignment": "warn",
		"prefer-const": "warn",
		"valid-jsdoc": "off",
		"yoda": "warn",
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


const configs = [configIgnores, configEslintRecommended];
Object.assign(configs[1].rules, configEslintExtra.rules);
configs[1].plugins = { ...configBracesRules.plugins };
Object.assign(configs[1].rules, configBracesRules.rules);
Object.assign(configs[1].plugins, configStylistic.plugins);
Object.assign(configs[1].rules, configStylistic.rules);

export default configs;
