
/*
Little XML library for C

Make XML tags with XML_tag()

XML my_xml = XML_tag("tag-name",
	"attr-name-1", "attr-value-1",
	"attr-name-2", "attr-value-2",
	NULL,  // Done with attributes
	"Some text & stuff in the tag",
	XML_tag("child-tag",
		NULL,  // No attributes
		NULL,  // No children
	),
	NULL // Done with children
)
As you see, you give the name of the tag as the first argument, followed by
pairs of strings for each attribute and each value.  Then give NULL to signify
the end of the attributes.  Then give the children of the tag, which are
either strings (plain text outside of tags) or more XML tags.  Finish with
NULL to signify the end of children.  Do not escape ampersands, angle brackets,
and quotes in the strings you give to this function.

You can turn the XML into a string with XML_as_text()
const char* text = XML_as_text(my_xml);
which give you a string containing:
<tag-name attr-name-1="attr-value-1" attr-name-2="attr-value-2">Some text &amp; stuff in the tag<child-tag/></tag-name>


You can find a tag that is a child of another tag by name with XML_get_child()
XML child = XML_get_child(my_xml, "child-tag")  // Yields <child-tag/>

You can get the value of an attribute of a tag by name with XML_get_attr()
const char* val = XML_get_attr(my_xml, "attr-name-2")  // Yields "attr-value-2"

TODO: Parsing a string into XML will come later.


*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <gc/gc.h>


typedef union XML XML;

typedef struct XML_Attr {
	const char* name;
	const char* value;
} XML_Attr;

typedef struct XML_Tag {
	uint is_str;
	const char* name;
	uint n_attrs;
	XML_Attr* attrs;
	uint n_contents;
	XML* contents;
} XML_Tag;

union XML {
	XML_Tag* tag;
	const char* str;
};

uint XML_is_str (XML);
uint XML_strlen (XML);
const char* XML_escape (const char*);
const char* XML_unescape (const char*);
const char* XML_as_text (const char*);
const char* XML_get_attr (XML, const char*);
XML XML_get_child (XML, const char*);


uint XML_is_str (XML xml) { return xml.tag->is_str; }

uint XML_strlen (XML xml) {
	uint r = 0;
	if (XML_is_str(xml)) {
		uint i;
		for (i = 0; xml.str[i]; i++) {
			switch (xml.str[i]) {
				case '<':
				case '>': { r += 4; break; }  // &lt; &gt;
				case '&': { r += 5; break; }  // &amp;
				case '"': { r += 6; break; }  // &quot;
				default: { r += 1; break; }
			}
		}
		return r;
	}
	else if (xml.tag->n_contents) {  // <tag></tag>
		r = 5;
		r += 2 * strlen(xml.tag->name);
		uint i;
		for (i = 0; i < xml.tag->n_contents; i++) {
			r += XML_strlen(xml.tag->contents[i]);
		}
	}
	else {  // <tag/>
		r = 3;
		r += strlen(xml.tag->name);
	}
	uint i;
	for (i = 0; i < xml.tag->n_attrs; i++) {
		r += 4;  // (space)name="value"
		r += strlen(xml.tag->attrs[i].name);
		r += XML_strlen((XML)xml.tag->attrs[i].value);
	}
	return r;
}

const char* XML_escape (const char* in) {
	uint len = XML_strlen((XML)in);
	uint i;
	uint xi;
	char* r = GC_malloc(len + 1);
	for (i = 0, xi = 0; in[i]; i++) {
		switch (in[i]) {
			case '<': { memcpy(r, "&lt;", 4); xi += 4; break; }
			case '>': { memcpy(r, "&gt;", 4); xi += 4; break; }
			case '&': { memcpy(r, "&amp;", 5); xi += 5; break; }
			case '"': { memcpy(r, "&quot;", 6); xi += 6; break; }
			default: { r[xi++] = in[i]; break; }
		}
	}
	r[xi] = 0;
	return (const char*)r;
}

const char* XML_unescape (const char* in) {
	char* r = GC_malloc(strlen(in) + 1);  // We can afford to be sloppy
	uint i;
	uint ri;
	for (i = 0, ri = 0; in[i]; i++, ri++) {
		r[ri] = in[i];
		if (in[i] == '&') {
			if (in[i+1] == 'l') {
				if (in[i+2] == 't') {
					if (in[i+3] == ';') {
						r[ri] = '<'; i += 3;
					}
				}
			}
			else if (in[i+1] == 'g') {
				if (in[i+2] == 't') {
					if (in[i+3] == ';') {
						r[ri] = '>'; i += 3;
					}
				}
			}
			else if (in[i+1] == 'a') {
				if (in[i+2] == 'm') {
					if (in[i+3] == 'p') {
						if (in[i+4] == ';') {
							r[ri] = '&'; i += 4;
						}
					}
				}
			}
			else if (in[i+1] == 'q') {
				if (in[i+2] == 'u') {
					if (in[i+3] == 'o') {
						if (in[i+4] == 't') {
							if (in[i+5] == ';') {
								r[ri] = '"'; i += 5;
							}
						}
					}
				}
			}
		}
	}
	r[ri] = 0;
	return (const char*)r;
}

const char* XML_as_text (XML xml) {
	if (XML_is_str(xml)) {
		return XML_escape(xml.str);
	}
	else {
		char* r = GC_malloc(XML_strlen(xml) + 1);
		uint ri = 0;
		r[ri++] = '<';
		uint i;
		uint namelen = strlen(xml.tag->name);
		memcpy(r+ri, xml.tag->name, namelen);
		ri += namelen;
		for (i = 0; i < xml.tag->n_attrs; i++) {
			r[ri++] = ' ';
			uint attrnamelen = strlen(xml.tag->attrs[i].name);
			memcpy(r+ri, xml.tag->attrs[i].name, attrnamelen);
			ri += attrnamelen;
			r[ri++] = '=';
			r[ri++] = '"';
			const char* attrvalue = XML_escape(xml.tag->attrs[i].value);
			uint attrvaluelen = strlen(attrvalue);
			memcpy(r+ri, attrvalue, attrvaluelen);
			ri += attrvaluelen;
			r[ri++] = '"';
		}
		if (xml.tag->n_contents) {
			r[ri++] = '>';
			for (i = 0; i < xml.tag->n_contents; i++) {
				const char* content = XML_as_text(xml.tag->contents[i]);
				uint contentlen = strlen(content);
				memcpy(r+ri, content, contentlen);
				ri += contentlen;
			}
			r[ri++] = '<';
			r[ri++] = '/';
			memcpy(r+ri, xml.tag->name, namelen);
			ri += namelen;
			r[ri++] = '>';
		}
		else {
			r[ri++] = '/';
			r[ri++] = '>';
		}
		r[ri] = 0;
		return r;
	}
}


XML XML_tag (const char* name, ...) {
	va_list args;
	va_start(args, name);
	va_list args_counter = args;
	uint n_attrs = 0;
	while (va_arg(args_counter, const char*)) n_attrs++;
	if (n_attrs % 2) {
		fprintf(stderr, "Error: Odd number of strings given in XML attribute list\n");
		exit(1);
	}
	n_attrs /= 2;
	uint n_contents = 0;
	while (va_arg(args_counter, void*)) n_contents++;
	XML_Tag* r = GC_malloc(sizeof(XML_Tag));
	r->is_str = 0;
	r->name = name;
	r->n_attrs = n_attrs;
	r->attrs = GC_malloc(n_attrs * sizeof(XML_Attr));
	uint i;
	for (i = 0; i < n_attrs; i++) {
		r->attrs[i].name = va_arg(args, const char*);
		r->attrs[i].value = va_arg(args, const char*);
	}
	va_arg(args, const char*);  // skip separating NULL
	r->n_contents = n_contents;
	r->contents = GC_malloc(n_contents * sizeof(XML));
	for (i = 0; i < n_contents; i++) {
		r->contents[i] = (XML)va_arg(args, XML_Tag*);
	}
	va_end(args);
	return (XML)r;
}

const char* XML_get_attr (XML xml, const char* name) {
	uint i;
	for (i = 0; i < xml.tag->n_attrs; i++)
	if (0==strcmp(xml.tag->attrs[i].name, name))
		return xml.tag->attrs[i].value;
	return NULL;
}
XML XML_get_child (XML xml, const char* name) {
	uint i;
	for (i = 0; i < xml.tag->n_contents; i++)
	if (!XML_is_str(xml.tag->contents[i]))
	if (0==strcmp(xml.tag->contents[i].tag->name, name))
		return xml.tag->contents[i];
	return (XML)(XML_Tag*)NULL;
}

/*
XML XML_test () {
	return
	XML_tag("wwxtp", NULL,
		XML_tag("query", NULL,
			XML_tag("command", NULL, "TEST", NULL),
			XML_tag("position", "lat", "23.001451", "long", "-15.23245", NULL, NULL),
			NULL
		),
		NULL
	);
}


int main () {
	XML myxml = XML_test();
	const char* xmlstr = XML_as_text(myxml);
	printf("name: %s, n_attrs: %u, n_contents: %u\n", myxml.tag->name, myxml.tag->n_attrs, myxml.tag->n_contents);
	printf("%u\n", XML_strlen(myxml));
	printf("%u\n", strlen(xmlstr));
	puts(xmlstr);
	XML mych = XML_get_child(myxml, "query");
	mych = XML_get_child(mych, "position");
	puts(XML_get_attr(mych, "long"));
	return 0;
}
*/

