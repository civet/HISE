/*  ===========================================================================
 *
 *   This file is part of HISE.
 *   Copyright 2016 Christoph Hart
 *
 *   HISE is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   HISE is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with HISE.  If not, see <http://www.gnu.org/licenses/>.
 *
 *   Commercial licenses for using HISE in an closed source project are
 *   available on request. Please visit the project's website to get more
 *   information about commercial licensing:
 *
 *   http://www.hise.audio/
 *
 *   HISE is based on the JUCE library,
 *   which must be separately licensed for closed source applications:
 *
 *   http://www.juce.com
 *
 *   ===========================================================================
 */

namespace hise
{

namespace multipage {
using namespace juce;

static constexpr char propertyCSS[4865] = R"(*
{
	color: #ddd;
	font-size: 17px;
	font-family: 'Lato';
}

/** Global properties (font, background, etc). */
body
{
	background: #282828;
	--global-padding: 10px;
	--triangle-icon: "66.t01PhrCQTd7bCwF..VDQTd7bCwF..ZBQzvgvCwF..d.QTd7bCwVccGAQTd7bCwF..ZBQEZepCw1PhrCQTd7bCMVY";
}

#header,
#footer
{
	display: none;
}

#content
{
	padding: 5px;
}

input, select
{
	
	background: #999;
	border-radius: 3px;
	border: 1px solid #aaa;
	margin: 2px;
	color: #111;
	text-align: left;
	padding-left: 8px;
	padding-top: 3px;
}

input:focus
{
	border: 2px solid;
	
	/** Getting a variable doesn't work in a multiproperty line
	    so we need to set the border-color property manually. */
	border-color: var(--headlineColour);
}

select::after
{
	content: '';
	background: #333;
	width: 100vh;
	background-image: var(--triangle-icon);
	margin: 8px;
}

select:hover
{
	color: #222;
}

select::after:hover
{
	background: #555;
}

::selection
{
	background: var(--headlineColour);
	color: black;
}

button
{
	background: #282828;
	color: transparent;
	width: 32px;
	margin: 0px;
	box-shadow: none;
	border: 0px;
}

button:hover
{
	background-color: #282828;
}


button::before
{
	position: absolute;
	content: '';
	width: 45px;
	margin: 6px;
	left: 0px;
	border-radius: 50%;
	border: 2px solid #ccc;
	background: transparent;
	box-shadow: 0px 3px 8px rgba(0, 0, 0, 0.3);
}

button::before:hover
{
	border: 2px solid white;
	transition: background 0.2s;
	background: rgba(255, 255, 255, 0.1);
	transform: scale(104%);
}

button::before:active,
button::before:active:checked
{
	transform: scale(99%);
}

button::before:checked
{
	transform: scale(99%);
	background: #90FFB1;
	box-shadow: inset 0px 2px 8px black;
}

button::after
{
	position: absolute;
	content: '';
	left: 0px;
	width: 100vh;
	margin: 10px;
	border-radius: 50%;	
	background: #ccc;
}

button::after:checked
{
	background: white;

	left: 13px;
	transition: left 0.2s;
}

div
{
	gap: 5px;
}

label
{
	text-align: left;
	color: #999;
	font-size: 13px;
	min-width: 70px;
}

.fold-bar
{
	margin: 0px;
	margin-bottom: 10px;
	width: 100%;
	height: 34px;
	font-weight: 500;
	background: #202020;
	border-radius: 5px 5px 0px 0px;
	border: 0px;
}

.fold-bar:checked
{
	background: #202020;
	border-radius: 5px;
}

.fold-bar:hover
{
	background: #242424;
}

.fold-bar::before
{
	/** required so that the element shows up */
	content: '';
	position: absolute;
	width: 100vh;
	background-color: #555;
	background-image: var(--triangle-icon);
	margin: 6px;
	transform: none;
}

.fold-bar::before:hover
{
	background-color: #999;
}

.fold-bar::before:checked
{
	transform: rotate(-90deg);
	transition: transform 0.2s ease-in;
}

.help-button
{
	order: 1000;
	height: 20px;
	width: 24px;
}

/** Popup-Menu styling */

/** This CSS class defines the background of the popup menu. */
.popup
{
	background: linear-gradient(to bottom, #222, #161616);
	border: 1px solid #444;
	border-radius: 3px;
	
}

/** This CSS class defines the default popup menu item style. */
.popup-item
{
	border: 0px solid transparent;
	font-size: 14px;
	font-family: Lato;
	background: transparent;
	color: #ddd;
	text-align: left;
	padding: 3px 15px;	
	margin: 3px;
	font-weight: 400;
}

/** The popup menu item that is currently hovered. */
.popup-item:hover
{
	background: rgba(255,255,255, 0.05);
}

/** The currently ticked popup menu item */
.popup-item:active
{
	color: white;
	font-weight: 500;
}

/** A disabled popup menu item */
.popup-item:disabled
{
	color: #555;
}

/** This is the popup menu header (the pseudo element focus is used for this) */
.popup-item:focus
{
	font-weight: 500;
}

/** This is the triangle indicating a submenu. */
.popup-item::after:root
{
	content: '';
	margin: 5px;
	width: 100vh;
	background: #888;
	background-image: var(--triangle-icon);
	transform: rotate(270deg);
}

/** Set the default after pseudo element to zero width */
.popup-item::after
{
	width: 0px;
}

/** Styling the popup menu separator. */
hr
{
	border: 1px solid #555;
}

.help-popup
{
	display: flex;
	height: auto;
	background: #161616;
	margin: 10px;
	padding: 15px;
	margin-top: 15px;
	border-radius: 5px;
	border: 1px solid #353535;
	box-sizing: border-box;
	box-shadow: 0px 0px 5px rgba(0, 0, 0, 0.2);
}

.help-popup::before
{
	background: #161616;
	background-image: "39.t0F++YBQfAfeCwF..VDQR+OuCwF..d.QR+OuCwF++YBQfAfeCMVY";
	content: '';
	width: 20px;
	height: 12px;
	position: absolute;
	top: -10px;
	left: calc(50% - 10px);
}

.help-close
{
	position: absolute;
	width: 18px;
	height: 18px;
	right: 0px;
	top: 0px;
}
)";

static constexpr char darkCSS[8607] = R"(
*
{
	opacity: 1.0;
	color: var(--textColour);
	font-size: 24px;
}

*:disabled
{
	opacity: 0.5;
}

/** Global properties (font, background, etc). */
body
{
	/** Pickup the font from the global selector. */
	font-family: var(--Font);
	
	/** Pickup the font size from the global selector. */
	font-size: var(--FontSize);
	
	background: #333;
	
	/** This is used for all global containers to get a consistent padding. */
	--global-padding: 30px;
	
	--triangle-icon: "66.t01PhrCQTd7bCwF..VDQTd7bCwF..ZBQzvgvCwF..d.QTd7bCwVccGAQTd7bCwF..ZBQEZepCw1PhrCQTd7bCMVY";
	
}


#header
{
	background-color: #282828;
	height: auto;
	padding: var(--global-padding);

	display: flex;
	flex-direction: column;
	
	/** aligns to the left */
	align-items: flex-start; 
	
	transform: none;
	/** create a shadow */
	box-shadow: inset 0px 0px 5px rgba(0, 0, 0, 0.7);
}



#content
{
	padding: var(--global-padding);
	border-top: 1px solid #444;
	
}


#title
{
	font-size: 2.0em;
	font-weight: 500;
	padding-bottom: 5px;
	
	/** Use the color from the global properties */
	color: var(--headlineColour);
}



#footer
{
	gap: 5px;
	padding: var(--global-padding);
	height: auto;
	margin: 0px;
	
	background: #222;
	box-shadow: inset 0px 0px 5px rgba(0, 0, 0, 0.5);
} 

button
{
	padding: 10px 20px;
	background: #444;
	border-radius: 3px;
	margin: 2px;
	border: 1px solid #555;
	box-shadow: 0px 2px 3px rgba(0, 0, 0, 0.2);
}

button:hover
{
	background: #555;
	transition: all 0.1s ease-in-out;
}

button:active
{
	box-shadow: none;
	transform: translate(0px, 1px);
	
}

input, select
{
	height: 40px;
	background: #999;
	border-radius: 3px;
	border: 1px solid #aaa;
	margin: 2px;
	color: #111;
	text-align: left;
	padding-left: 8px;
	padding-top: 3px;
}

input:focus
{
	border: 2px solid;
	
	/** Getting a variable doesn't work in a multiproperty line
	    so we need to set the border-color property manually. */
	border-color: var(--headlineColour);
}

select::after
{
	content: '';
	background: #333;
	width: 100vh;
	background-image: var(--triangle-icon);
	margin: 10px;
}

select:hover
{
	color: #333;
}

select::after:hover
{
	background: #555;
}

::selection
{
	background: var(--headlineColour);
	color: black;
}

.toggle-button
{
	background: #282828;
	color: transparent;
	width: 32px;
	margin: 0px;
	box-shadow: none;
	border: 0px;
}

.toggle-button:hover
{
	background-color: #282828;
}

.toggle-button:checked
{
	
}

.toggle-button::before
{
	position: absolute;
	content: '';
	width: 32px;
	margin: 6px;
	right: 0px;
	border-radius: 5px;
	border: 2px solid #ccc;
	background: transparent;
	box-shadow: 0px 3px 8px rgba(0, 0, 0, 0.3);
}

.toggle-button::before:hover
{
	border: 2px solid white;
	transition: background 0.5s;
	background: rgba(255, 255, 255, 0.1);
	transform: scale(104%);
}

.toggle-button::before:active
{
	transform: scale(99%);
}

.toggle-button::after
{
	position: absolute;

	content: '';
	right: 0px;
	width: 100vh;
	margin: 10px;
	border-radius: 2px;	
	background: transparent;
}

.toggle-button::after
{
	background: transparent;
}

.toggle-button::after:checked
{
	background: #ccc;
	
}

label
{
	

	min-width: 150px;
	text-align: right;
}


/** Styling of the fold bar (the clickable area of a list that
    hides its children if `Foldable` is enabled)
    
    The element is a button so we need to override anything that
    is defined in the default button class!
*/

.fold-bar
{
	margin: 0px;
	margin-bottom: 10px;
	width: 100%;
	height: 34px;
	font-weight: 500;
	background: #202020;
	border-radius: 5px 5px 0px 0px;
	border: 0px;
}

.fold-bar:checked
{
	background: #202020;
	border-radius: 5px;
}

.fold-bar:hover
{
	background: #242424;
}

.fold-bar::before
{
	/** required so that the element shows up */
	content: '';
	position: absolute;
	width: 100vh;
	background-color: #555;
	background-image: var(--triangle-icon);
	margin: 6px;
	transform: none;
}

.fold-bar::before:hover
{
	background-color: #999;
}

.fold-bar::before:checked
{
	transform: rotate(-90deg);
	transition: transform 0.2s ease-in;
}

.help-button, .retry-button
{
	order: 1000;
	height: 24px;
	width: 32px;
}

/** This is the appearance of all progress bars
    that indicate a background process. */
    
progress
{
	margin: 4px;
	height: 36px;
	background: #444;
	border-radius: 50%;
	border: 1px solid #999;
	box-shadow: 0px 1px 3px rgba(0, 0, 0, 0.5);
	color: white;
	font-size: 14px;
	font-weight: 500;
}

progress::before
{
	content: '';
	height: 100%;
	border-radius: 50%;
	margin: 4px;
	position: absolute;
	background: linear-gradient(to bottom, #999, #888);
	width: var(--progress);
}

/** Now we skin the top progress bar */
#total-progress
{
	box-shadow: none;
	border: 0px;
	margin: 0px;
	background: transparent;
	color: #777;
	height: 24px;
	width: 100%;
	font-size: 14px;
	vertical-align: top;
	text-align: right;
}

#total-progress::after:hover
{
	background: white;
	transition: all 0.4s ease-in-out;
}

#total-progress::before
{
	position: absolute;
	margin: 0px;
	content: '';
	width: 100%;
	height: 4px;
	top: 20px;
	background: #181818;
	border-radius: 2px;
}

#total-progress::after
{
	position: absolute;
	left: 2px;
	top: 21px;
	
	content: '';
	width: var(--progress);
	background: #ddd;
	height: 2px;
	max-width: calc(100% - 4px);
	border-radius: 1px;
	box-shadow: 0px 0px 3px rgba(255, 255, 255, 0.1);
	
}

/** Popup-Menu styling */

/** This CSS class defines the background of the popup menu. */
.popup
{
	background: linear-gradient(to bottom, #222, #161616);
	border: 1px solid #444;
	border-radius: 3px;
}

/** This CSS class defines the default popup menu item style. */
.popup-item
{
	border: 0px solid transparent;
	background: transparent;
	color: #ddd;
	text-align: left;
	padding: 3px 15px;	
	margin: 3px;
}

/** The popup menu item that is currently hovered. */
.popup-item:hover
{
	background: rgba(255,255,255, 0.05);
}

/** The currently ticked popup menu item */
.popup-item:active
{
	color: white;
	font-weight: 500;
}

/** A disabled popup menu item */
.popup-item:disabled
{
	color: #555;
}

/** This is the popup menu header (the pseudo element focus is used for this) */
.popup-item:focus
{
	font-weight: 500;
	font-size: 1.1em;
}

/** This is the triangle indicating a submenu. */
.popup-item::after:root
{
	content: '';
	margin: 5px;
	width: 100vh;
	background: #888;
	background-image: var(--triangle-icon);
	
	transform: rotate(270deg);
	
	
}

/** Set the default after pseudo element to zero width */
.popup-item::after
{
	width: 0px;
}

/** Styling the popup menu separator. */
hr
{
	border: 1px solid #555;
}

.error
{
	padding: 5px;
	border: 1px solid #e44;
	border-radius: 4px;
	background: rgba(255, 0, 0, 0.05);
	padding-right: 40px;
}

.error::after
{
	content: '';
	background: #e44;
	width: 20px;
	height: 20px;
	top: calc(50% - 10px);
	right: 10px;
	background-image: "230.t0F6+YBQ9++OCIl9adCQ9++OCA.fEQD6Od2P..XQDc8+cNjX..XQDM+M.Oj9adCQ...2Cw9elPD..v8PhI.YUPD..v8P..3ADM+M.OD..d.QW+emCIF..d.Qr+3cCI.YUPj+++yPr+mID4+++LzXsIBplPzgiO4Prg84VPDtEi1PrQcVRPjl8q2PrAiFhPjR+y4PrQcVRPjT.x6Prg84VPzMbV7PrIBplPznaX5Pr0FZ1PzMbV7PrAl85PjT.x6PrUgMqPjR+y4PrAl85Pjl8q2Pr0FZ1PDtEi1PrIBplPzgiO4PiUF";
	box-shadow: 0px 0px 5px black;
	
}

.help-popup
{
	display: flex;
	height: auto;
	background: #161616;
	margin: 10px;
	padding: 15px;
	margin-top: 15px;
	border-radius: 5px;
	border: 1px solid #353535;
	box-sizing: border-box;
	box-shadow: 0px 0px 5px rgba(0, 0, 0, 0.2);
}

.help-popup::before
{
	background: #161616;
	background-image: "39.t0F++YBQfAfeCwF..VDQR+OuCwF..d.QR+OuCwF++YBQfAfeCMVY";
	content: '';
	width: 20px;
	height: 12px;
	position: absolute;
	top: -10px;
	left: calc(50% - 10px);
}

.help-close
{
	position: absolute;
	width: 18px;
	height: 18px;
	right: 0px;
	top: 0px;
}

h1, h2, h3, h4
{
	font-size: 2.3rem;
	margin-bottom: 0px;

	
}

.modal-bg
{
	position: absolute;
	background: rgba(25,25,25, 0.8);
}

.modal-popup
{
	border: 1px solid #555;
	border-radius: 3px;
	box-shadow: 0px 2px 4px black;
}

.big-button
{
	
}

.small-button
{
	background: red;
	border: 4px solid blue;
	padding: 5px;
	color: black;
}

)";

static constexpr char brightCSS[2630] = R"(
*
{
	color: #222;
	font-family: 'Lato';
	font-size: 16px;
	--funky: red;
}



#header
{
	background: linear-gradient(to bottom, #ddd, #bbb);
	box-shadow: inset 0px -2px 10px rgba(0, 0, 0, 0.2);
	height: 80px;
	display: flex;
	flex-direction: column;
	align-items: flex-start;
	padding: 20px;
	border-bottom: 1px solid #777;
}

#title
{
	font-size: 24px;
	font-weight: 500;
}

body
{
	background: #999;
}

#content
{
	padding: 30px;
}

#footer
{
	background: #222;
	box-shadow: inset 0px 2px 3px black;
	gap: 10px;
	padding: 20px;
}

button
{
	font-size: 16px;

	background: #555;
	padding: 5px 10px;
	border: 1px solid #666;
	margin: 4px;
	box-shadow: 0px 1px 3px rgba(0, 0, 0, 0.4);
	border-radius: 3px;
	color: #aaa;
}

button:hover
{
	background: #666;
}

.toggle-button
{
	background: transparent;
	box-shadow: none;
	border: 0px;
	color: transparent;
	text-align: left;
	padding-left: 52px;
	height: 40px;
}

.toggle-button:hover
{
	background: rgba(0, 0, 0, 0.05);
	border-radius: 50%;
	transition: background 0.1s;
	
}

.toggle-button::after
{
	content: '';
	width: 30px;
	height: 30px;
	background: linear-gradient(to bottom, #ddd, #bbb);
	
	left: 0px;
	margin:7px;
	border-radius: 50%;
}

.toggle-button::after:checked
{
	content: '';
	width: 30px;
	height: 30px;
	
	left: 20px;
	margin:7px;
	border-radius: 50%;
	transition: left 0.2s ease-in-out;
}

.toggle-button::before
{
	position: absolute;
	box-shadow: inset 0px 2px 4px rgba(0, 0, 0, 0.2);
	border: 2px solid rgba(0, 0, 0, 0.2);
	content: '';
	width: 50px;
	height: 30px;
	left: 0px;
	background: #888;
	margin: 5px;
	border-radius: 50%;
}

.toggle-button::before:checked
{
	background: #4C6F8E;
}



input
{
	box-shadow: inset 0px 2px 4px rgba(0, 0, 0, 0.2);
	border: 2px solid rgba(0, 0, 0, 0.2);
	content: '';
	height: 40px;
	left: 0px;
	background: #aaa;
	margin: 5px;
	padding-top: 0px;
	padding-left: 10px;
	border-radius: 5px;
}

input:focus
{
	border: 3px solid #4C6F8E;
	background: #ddd;
	transition: background 0.4s;
}

::selection
{
	background: #4C6F8E;
	color: white;
}

h1
{
	color: #4C6F8E;
	font-size: 2.0em;
}

#activate
{
	width: 150px;
}

.tag-button label
{
	display: none;
}

.tag-button button
{
	background: #444;
	margin: 5px;
	
	border-radius: 50%;
	color: white;
	text-align: center;
	padding: 0px 15px;
	flex-grow: 0;
}

.tag-button button::before,
.tag-button button::after,
.tag-button button::after:checked
{
	width: 0px;
	height: 0px;
}
)";

String DefaultCSSFactory::getTemplate(Template t)
{
	switch(t)
	{
	case Template::PropertyEditor: return String(propertyCSS);
	case Template::Dark: return String(darkCSS);
	case Template::Bright: return String(brightCSS);
	case Template::numTemplates: break;
	default: ;
	}

	return {};
}

simple_css::StyleSheet::Collection DefaultCSSFactory::getTemplateCollection(Template t)
{
	using namespace simple_css;

	Parser p(getTemplate(t));
	p.parse();
	return p.getCSSValues();
}

}
}