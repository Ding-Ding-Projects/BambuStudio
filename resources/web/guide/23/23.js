
var m_ProfileItem;
var searchTags = [];
var searchTimer = null;
var customFilaments = [];

var FilamentPriority=new Array( "pla","abs","pet","tpu","pc");
var VendorPriority=new Array("bambu lab","bambulab","bbl","kexcelled","polymaker","esun","generic");

function OnInit()
{
	[['#printerBtn', '#MachineList'], ['#filatypeBtn', '#FilatypeList'],
		['#vendorBtn', '#VendorList']].forEach(function(pair) {
		$(pair[0]).on('click', function() {
			const expanded = $(this).attr('aria-expanded') === 'true';
			$(this).attr('aria-expanded', String(!expanded));
			$(this).find('.CArrow').toggleClass('active', expanded);
			$(pair[1]).slideToggle(300);
		}).on('keydown', function(event) {
			if (event.key === 'Enter' || event.key === ' ') {
				event.preventDefault();
				$(this).trigger('click');
			}
		});
	});

  $('#SelectAllCheckbox').change(function() {
    if ($(this).is(':checked')) {
      SelectAllFilament(1);
    } else {
      SelectAllFilament(0);
    }
  });
	let composing = false;
	$('#filamentSearch').on('compositionstart', function() { composing = true; })
		.on('compositionend', function() { composing = false; SortFilament(); })
		.on('input', function() {
			if (composing) return;
			clearTimeout(searchTimer);
			searchTimer = setTimeout(SortFilament, 150);
		}).on('keydown', function(event) {
			if (event.key === 'Enter' && !composing) {
				event.preventDefault();
				addSearchTag();
			}
		});
	$('#addSearchTag').on('click', addSearchTag);
	$('#CFilament_Sort').on('change', renderCustomFilaments);
	TranslatePage();
  OnSelectMenu(GetQueryString('custom') === '1' ? 2 : 1);
	
	RequestProfile();
	
	RequestCustomFilaments();
	//TestCustomFilaments();
	//OnSelectMenu(2);
}

function RequestProfile()
{
	var tSend={};
	tSend['sequence_id']=Math.round(new Date() / 1000);
	tSend['command']="request_userguide_profile";
	
	SendWXMessage( JSON.stringify(tSend) );
}

//function RequestModelSelect()
//{
//	var tSend={};
//	tSend['sequence_id']=Math.round(new Date() / 1000);
//	tSend['command']="request_userguide_modelselected";
//	
//	SendWXMessage( JSON.stringify(tSend) );
//}

function HandleStudio(pVal)
{
	let strCmd=pVal['command'];
	//alert(strCmd);
	
	if(strCmd=='response_userguide_profile')
	{
		m_ProfileItem=pVal['response'];
		SortUI();
	}
	else if(strCmd=='update_custom_filaments')
	{
		UpdateCustomFilaments( pVal['data'] );
	}
}

function GetFilamentShortname( sName )
{
	let sShort=sName.split('@')[0].trim();
	
	return sShort;
}

function addSearchTag()
{
	const term = String($('#filamentSearch').val() || '').trim().toLowerCase();
	if (term && !searchTags.includes(term)) searchTags.push(term);
	$('#filamentSearch').val('').trigger('focus');
	renderSearchTags();
	SortFilament();
}

function renderSearchTags()
{
	const list = $('#searchTags').empty();
	searchTags.forEach(function(term) {
		$('<button type="button">').addClass('searchTag')
			.attr('aria-label', GetCurrentPlainTextByKey('t88') + ' ' + term)
			.text(term + ' ×').on('click', function() {
				searchTags = searchTags.filter(item => item !== term);
				renderSearchTags();
				SortFilament();
			}).appendTo(list);
	});
}


function SortUI()
{
  const models = (m_ProfileItem.model || []).filter(model => model.nozzle_selected);
  const filaments = Object.entries(m_ProfileItem.filament || {});
  const types = new Map();
  const vendors = new Map();
  const rows = new Map();
  $('#MachineList .filter-option, #FilatypeList .filter-option, #VendorList .filter-option').remove();
  $('#ItemBlockArea').empty();
  for (const model of models) {
    appendFilterOption('#MachineList', String(model.model || ''), 'mode', String(model.nozzle_selected || ''), MachineClick);
  }
  $('#MachineList input').prop('checked', true);
  if (models.length <= 1) {
    $('#MachineList').hide();
    $('#printerBtn').attr('aria-expanded', 'false');
  } else {
    $('#MachineList').show();
    $('#printerBtn').attr('aria-expanded', 'true');
  }
  for (const [profileKey, filament] of filaments) {
    const wholeName = String(filament.name || profileKey).trim();
    const shortName = GetFilamentShortname(wholeName);
    const vendor = String(filament.vendor || '');
    const type = String(filament.type || '');
    const compatibility = String(filament.models || '');
    const compatible = !compatibility || models.some(model =>
      String(model.nozzle_selected).split(';').some(nozzle =>
        compatibility.includes('[' + model.model + '++' + nozzle + ']')));
    if (!compatible) continue;
    types.set(type.toLowerCase(), type);
    vendors.set(vendor.toLowerCase(), vendor);
    const key = JSON.stringify([vendor, type, shortName]);
    if (!rows.has(key)) {
      const input = $('<input type="checkbox">').attr({
        vendor: vendor, filatype: type, name: shortName
      }).on('change', updateSelectAllCheckbox);
      // The name is shown in the ink wording; the attribute and the profile key keep the preset's own name.
      const nameText = $('<span>').text(DisplayInkWording(shortName));
      if (DisplayInkWordingTitle(shortName)) nameText.attr('title', DisplayInkWordingTitle(shortName));
      const row = $('<label>').addClass('filament-row').append(input, nameText);
      row.data('models', []).data('filamentKeys', []);
      rows.set(key, row);
      $('#ItemBlockArea').append(row);
    }
    const row = rows.get(key);
    row.data('models').push(compatibility);
    row.data('filamentKeys').push(profileKey);
    if (Number(filament.selected) === 1) row.find('input').prop('checked', true);
  }
  function ordered(map, priority) {
    return [...map].sort((a, b) => {
      const ap = priority.indexOf(a[0]), bp = priority.indexOf(b[0]);
      if (ap !== bp) return (ap < 0 ? 999 : ap) - (bp < 0 ? 999 : bp);
      return a[1].localeCompare(b[1]);
    }).map(entry => entry[1]);
  }
  ordered(types, FilamentPriority).forEach(type =>
    appendFilterOption('#FilatypeList', type, 'filatype', '', FilaClick));
  ordered(vendors, VendorPriority).forEach(vendor =>
    appendFilterOption('#VendorList', vendor, 'vendor', '', VendorClick));
  $('#FilatypeList input, #VendorList input').prop('checked', true);
  if ($('#ItemBlockArea input:checked').length === 0) ChooseDefaultFilament();
  SortFilament();
}

function appendFilterOption(parent, value, attribute, extra, handler)
{
  const input = $('<input type="checkbox">').addClass('inputIndent').attr(attribute, value)
    .on('change', handler);
  if (attribute === 'mode') input.attr('nozzle', extra);
  // The value stays in the attribute (it is what the filters compare); only the shown text changes.
  const text = $('<span>').text(DisplayInkWording(value));
  if (DisplayInkWordingTitle(value)) text.attr('title', DisplayInkWordingTitle(value));
  $('<label>').addClass('checkboxText filter-option').append(input, text)
    .appendTo(parent);
}

function ChooseAllMachine()
{
	let bCheck=$("#MachineList input:first").prop("checked");
	
	$("#MachineList input").prop("checked",bCheck);
	
	SortFilament();
}

function MachineClick()
{
	let nChecked=$("#MachineList input:gt(0):checked").length
	let nAll    =$("#MachineList input:gt(0)").length
	
	if(nAll==nChecked)
	{
		$("#MachineList input:first").prop("checked",true);
	}
	else
	{
		$("#MachineList input:first").prop("checked",false);
	}
	
	SortFilament();
}

function ChooseAllFilament()
{
	let bCheck=$("#FilatypeList input:first").prop("checked");	
	$("#FilatypeList input").prop("checked",bCheck);	
	
	SortFilament();
}

function FilaClick()
{
	let nChecked=$("#FilatypeList input:gt(0):checked").length
	let nAll    =$("#FilatypeList input:gt(0)").length
	
	if(nAll==nChecked)
	{
		$("#FilatypeList input:first").prop("checked",true);
	}
	else
	{
		$("#FilatypeList input:first").prop("checked",false);
	}
	
	SortFilament();	
}

function ChooseAllVendor()
{
	let bCheck=$("#VendorList input:first").prop("checked");	
	$("#VendorList input").prop("checked",bCheck);	
	
	SortFilament();
}

function VendorClick()
{
	let nChecked=$("#VendorList input:gt(0):checked").length
	let nAll    =$("#VendorList input:gt(0)").length
	
	if(nAll==nChecked)
	{
		$("#VendorList input:first").prop("checked",true);
	}
	else
	{
		$("#VendorList input:first").prop("checked",false);
	}
	
	SortFilament();
}



function SortFilament()
{
  const selectedModels = [];
  $('#MachineList input:gt(0):checked').each(function() {
    const model = $(this).attr('mode');
    for (const nozzle of String($(this).attr('nozzle') || '').split(';')) {
      if (nozzle) selectedModels.push('[' + model + '++' + nozzle + ']');
    }
  });
  const types = new Set($('#FilatypeList input:gt(0):checked').map(function() {
    return $(this).attr('filatype');
  }).get());
  const vendors = new Set($('#VendorList input:gt(0):checked').map(function() {
    return $(this).attr('vendor');
  }).get());
  const terms = searchTags.concat(String($('#filamentSearch').val() || '').trim().toLowerCase() || [])
    .filter(Boolean);
  let visible = 0;
  $('#ItemBlockArea .filament-row').each(function() {
    const row = $(this), input = row.find('input');
    const compatibility = row.data('models') || [];
    const modelMatch = selectedModels.length === 0 || compatibility.some(value =>
      !value || selectedModels.some(model => value.includes(model)));
    const typeMatch = types.size === 0 || types.has(input.attr('filatype'));
    const vendorMatch = vendors.size === 0 || vendors.has(input.attr('vendor'));
    // A search matches what the row shows as well as the stored name and type.
    const haystack = [input.attr('name'), DisplayInkWording(input.attr('name') || ''), input.attr('vendor'),
      input.attr('filatype'), DisplayInkWording(input.attr('filatype') || '')].join(' ').toLowerCase();
    const textMatch = terms.length === 0 || terms.some(term => haystack.includes(term));
    const show = (selectedModels.length + types.size + vendors.size > 0) &&
      modelMatch && typeMatch && vendorMatch && textMatch;
    row.toggle(show);
    if (show) visible++;
  });
  const format = GetCurrentPlainTextByKey('t254') || 'Filter results: {n} matches';
  $('#filterResultText').text(format.split('{n}').join(String(visible)));
  $('#filamentEmpty').prop('hidden', visible !== 0);
  updateFilterCount('MachineList', 'printerCount');
  updateFilterCount('FilatypeList', 'filatypeCount');
  updateFilterCount('VendorList', 'vendorCount');
  updateSelectAllCheckbox();
}

function updateFilterCount(list, count)
{
  $('#' + count).text('(' + $('#' + list + ' input:gt(0):checked').length +
    '/' + $('#' + list + ' input:gt(0)').length + ')');
}

function updateSelectAllCheckbox()
{
  const visible = $('#ItemBlockArea .filament-row:visible input');
  const selected = visible.filter(':checked').length;
  $('#SelectAllCheckbox').prop('checked', visible.length > 0 && selected === visible.length)
    .prop('indeterminate', selected > 0 && selected < visible.length);
}

function SelectAllFilament(select)
{
  $('#ItemBlockArea .filament-row:visible input').prop('checked', Boolean(select));
  updateSelectAllCheckbox();
}

function ChooseDefaultFilament()
{
  const models = new Set($('#MachineList input:gt(0)').map(function() {
    return $(this).attr('mode');
  }).get());
  const defaults = new Set();
  for (const model of (m_ProfileItem.model || [])) {
    if (models.has(model.model)) {
      String(model.materials || '').split(';').filter(Boolean).forEach(name => defaults.add(name));
    }
  }
  $('#ItemBlockArea .filament-row').each(function() {
    const names = $(this).data('filamentKeys') || [];
    $(this).find('input').prop('checked', names.some(name => defaults.has(name)));
  });
  ShowNotice(0);
}

function ShowNotice( nShow )
{
	if(nShow==0)
	{
		$("#NoticeMask").hide();
		$("#NoticeBody").hide();
	}
	else
	{
		$("#NoticeMask").show();
		$("#NoticeBody").show();
	}
}


function ResponseFilamentResult()
{
	let FilaSelectedList= $("#ItemBlockArea input:checked");
	let nAll=FilaSelectedList.length;

	if( nAll==0 )
	{
		ShowNotice(1);
		return false;
	}
	
	let FilaArray=new Array();
	let seen = new Set();
	for(let n=0;n<nAll;n++)
	{
		let names = $(FilaSelectedList[n]).closest('.filament-row').data('filamentKeys') || [];
		for (const name of names) {
			if (!seen.has(name)) {
				seen.add(name);
				FilaArray.push(name);
			}
		}
	}
	
	var tSend={};
	tSend['sequence_id']=Math.round(new Date() / 1000);
	tSend['command']="save_userguide_filaments";
	tSend['data']={};
	tSend['data']['filament']=FilaArray;
	
	SendWXMessage( JSON.stringify(tSend) );
	
	return true;
}


function CancelSelect()
{
	var tSend={};
	tSend['sequence_id']=Math.round(new Date() / 1000);
	tSend['command']="user_guide_cancel";
	tSend['data']={};
		
	SendWXMessage( JSON.stringify(tSend) );			
}


function ConfirmSelect()
{
	let bRet=ResponseFilamentResult();
	
	if(bRet)
    {
		var tSend={};
		tSend['sequence_id']=Math.round(new Date() / 1000);
		tSend['command']="user_guide_finish";
		tSend['data']={};
		tSend['data']['action']="finish";
		
		SendWXMessage( JSON.stringify(tSend) );			
	}
}


function OnSelectMenu( nIndex )
{
	switch(nIndex)
	{
		case 1:
			$('#SystemFilamentBtn').addClass('TitleSelected').removeClass('TitleUnselected').attr('aria-pressed', 'true');
			$('#CustomFilamentBtn').addClass('TitleUnselected').removeClass('TitleSelected').attr('aria-pressed', 'false');
			$('#SystemFilamentsArea').css('display','flex');
			$('#CustomFilamentsArea').css('display','none');
			updateSelectAllCheckbox();
			break;
		case 2:
			$('#CustomFilamentBtn').addClass('TitleSelected').removeClass('TitleUnselected').attr('aria-pressed', 'true');
			$('#SystemFilamentBtn').addClass('TitleUnselected').removeClass('TitleSelected').attr('aria-pressed', 'false');
			$('#CustomFilamentsArea').css('display','flex');
			$('#SystemFilamentsArea').css('display','none');			
			break;
	}
}

function RequestCustomFilaments()
{
	var tSend={};
	tSend['sequence_id']=Math.round(new Date() / 1000);
	tSend['command']="request_custom_filaments";
		
	SendWXMessage( JSON.stringify(tSend) );		
}

function TestCustomFilaments()
{
	let strTest='{"command":"update_custom_filaments","data":[{"id":"P0c71f94","name":"AMOLEN ABS 222"},{"id":"P19cc6c5","name":"PrimaSelect PLA 231654"},{"id":"P93a5c3b","name":"3DJAKE PLA 111"}],"sequence_id":"2000"}';
	let tItem=JSON.parse(strTest);
	
	HandleStudio(tItem);
}

function UpdateCustomFilaments(CFList)
{
  customFilaments = Array.isArray(CFList) ? CFList.slice() : [];
  renderCustomFilaments();
}

function renderCustomFilaments()
{
  const list = customFilaments.slice();
  const order = $('#CFilament_Sort').val() || 'newest';
  const countText = GetCurrentPlainTextByKey('t242') || 'Custom inks: 0';
  $('#CFilament_Count').text(countText.replace(/0/g, String(list.length)));
  $('#CFilament_Empty').prop('hidden', list.length !== 0);
  $('#CFilament_Btn_Area').toggle(list.length !== 0);
  $('#CFilament_Sort').prop('disabled', list.length === 0);
  const host = $('#CFilament_List').empty();
  if (!list.length) return;
  const name = item => String(item.name || '');
  const type = item => String(item.type || '');
  const date = item => String(item.create_time || item.date || '');
  if (order === 'type') {
    list.sort((a, b) => type(a).localeCompare(type(b)) || name(a).localeCompare(name(b)));
  } else {
    list.sort((a, b) => {
      const compare = date(a).localeCompare(date(b));
      return (order === 'oldest' ? compare : -compare) || name(a).localeCompare(name(b));
    });
  }
  function addGroup(title, items) {
    if (!items.length) return;
    const group = $('<section>').addClass('customGroup');
    const heading = $('<button type="button">').addClass('customGroupHeading')
      .attr('aria-expanded', 'true').text(title + ' (' + items.length + ')');
    const body = $('<div>').addClass('customGroupBody');
    heading.on('click', function() {
      const expanded = heading.attr('aria-expanded') === 'true';
      heading.attr('aria-expanded', String(!expanded));
      body.prop('hidden', expanded);
    });
    items.forEach(item => appendCustomRow(body, item));
    group.append(heading, body).appendTo(host);
  }
  if (order === 'newest') {
    const now = new Date();
    const today = new Date(now.getFullYear(), now.getMonth(), now.getDate()).getTime();
    const week = today - 7 * 86400000;
    const todayItems = [], weekItems = [], earlierItems = [];
    list.forEach(item => {
      const timestamp = Date.parse(date(item).replace(/-/g, '/'));
      if (Number.isFinite(timestamp) && timestamp >= today) todayItems.push(item);
      else if (Number.isFinite(timestamp) && timestamp >= week) weekItems.push(item);
      else earlierItems.push(item);
    });
    addGroup(GetCurrentPlainTextByKey('t255') || 'Today', todayItems);
    addGroup(GetCurrentPlainTextByKey('t256') || 'This week', weekItems);
    addGroup(GetCurrentPlainTextByKey('t257') || 'Earlier', earlierItems);
  } else if (order === 'type') {
    const groups = new Map();
    list.forEach(item => {
      const key = type(item) || GetCurrentPlainTextByKey('t245') || 'Ink type';
      if (!groups.has(key)) groups.set(key, []);
      groups.get(key).push(item);
    });
    groups.forEach((items, title) => addGroup(DisplayInkWording(title), items));
  } else {
    list.forEach(item => appendCustomRow(host, item));
  }
}

function appendCustomRow(host, item)
{
  const id = String(item.id || '');
  const name = String(item.name || '');
  const row = $('<div>').addClass('CFilament_Item');
  $('<span>').addClass('CFilament_Name').attr('title', name).text(name).appendTo(row);
  $('<span>').addClass('CFilament_Type').text(DisplayInkWording(String(item.type || ''))).appendTo(row);
  $('<span>').addClass('CFilament_Date').text(String(item.create_time || item.date || '').slice(0, 10)).appendTo(row);
  const edit = $('<button type="button">').addClass('CFilament_EditBtn')
    .attr('aria-label', GetCurrentPlainTextByKey('t128') + ' ' + name)
    .on('click', function() { CFEdit(id); });
  $('<img>').attr({src: '../../image/edit.svg', alt: ''}).appendTo(edit);
  row.append(edit);
  const remove = $('<button type="button">').addClass('CFilament_DeleteBtn')
    .attr('aria-label', GetCurrentPlainTextByKey('t88') + ' ' + name)
    .on('click', function() { CFDelete(id, name); });
  $('<img>').attr({src: '../../image/delete.svg', alt: ''}).appendTo(remove);
  row.append(remove);
  host.append(row);
}

function OnClickCustomFilamentAdd()
{
	//alert('Create New Custom Filament');
	
	var tSend={};
	tSend['sequence_id']=Math.round(new Date() / 1000);
	tSend['command']="create_custom_filament";
		
	SendWXMessage( JSON.stringify(tSend) );		
}

//编辑某一个自定义材料
function CFEdit( fid )
{
	//alert(fid);
	
	var tSend={};
	tSend['sequence_id']=Math.round(new Date() / 1000);
	tSend['command']="modify_custom_filament";
	tSend['id']=fid;
		
	SendWXMessage( JSON.stringify(tSend) );	
}

function CFDelete(fid, name)
{
	var tSend = {};
	tSend['sequence_id'] = Math.round(new Date() / 1000);
	tSend['command'] = 'delete_custom_filament';
	tSend['id'] = fid;
	tSend['name'] = name;
	SendWXMessage(JSON.stringify(tSend));
}


